// TatouSource/tests/engine/test_font_compose.cpp
#include "doctest.h"
#include "fontCompose.h"

#include <cstdint>
#include <vector>

namespace {
// A font in the ITD_RESS entry 5 layout (font.cpp SetFont): s16 align, u8 height,
// u8 stride, s16 0, big-endian u16 table offset, strip rows, then a big-endian u16
// per code from align (width << 12 | starting bit).
struct Glyph { int code, width; std::vector<const char*> rows; };

std::vector<unsigned char> makeFont(const std::vector<Glyph>& glyphs, int height = 6, int stride = 8)
{
    const int align = 32, table = 8 + height * stride;
    std::vector<unsigned char> f(table + (256 - align) * 2, 0);
    f[0] = align; f[2] = (unsigned char)height; f[3] = (unsigned char)stride;
    f[6] = (unsigned char)(table >> 8); f[7] = (unsigned char)(table & 0xFF);
    int bit = 0;
    for (const Glyph& g : glyphs)
    {
        for (int r = 0; r < height; r++)
            for (int x = 0; x < g.width; x++)
                if (g.rows[r][x] == '#')
                    f[8 + r * stride + ((bit + x) >> 3)] |= (unsigned char)(0x80 >> ((bit + x) & 7));
        const int e = table + (g.code - align) * 2, v = (g.width << 12) | bit;
        f[e] = (unsigned char)(v >> 8); f[e + 1] = (unsigned char)(v & 0xFF);
        bit += g.width;
    }
    return f;
}

std::vector<std::string> glyphRows(const std::vector<unsigned char>& f, int code)
{
    const int align = f[0], height = f[2], stride = f[3], table = (f[6] << 8) | f[7];
    const int e = table + (code - align) * 2, v = (f[e] << 8) | f[e + 1], w = v >> 12, bit = v & 0xFFF;
    std::vector<std::string> rows;
    for (int r = 0; r < height; r++)
    {
        std::string row;
        for (int x = 0; x < w; x++)
            row += (f[8 + r * stride + ((bit + x) >> 3)] & (0x80 >> ((bit + x) & 7))) ? '#' : '.';
        rows.push_back(row);
    }
    return rows;
}

const std::vector<Glyph> kFont = {
    { 'a', 4, { "....", "....", ".##.", "#.#.", ".###", "...." } },
    { 'o', 4, { "....", "....", ".##.", "#..#", ".##.", "...." } },
    { 0xA4, 5, { ".#.#.", "#.#..", ".....", "##...", "#.#..", "....." } },   // tilde, gap, n: like the real font
    { 'A', 4, { ".##.", "#..#", "####", "#..#", "#..#", "...." } },
    { 'E', 4, { "####", "#...", "###.", "#...", "####", "...." } },
    { 'I', 2, { "##", "##", "##", "##", "##", ".." } },
    { 'O', 4, { ".##.", "#..#", "#..#", "#..#", ".##.", "...." } },
    { 'U', 4, { "#..#", "#..#", "#..#", "#..#", ".##.", "...." } },
    { 'z', 3, { "...", "...", "###", ".#.", "###", "..." } },
};
}

TEST_CASE("composeFont puts the tilde of n-tilde over a and o")
{
    const auto font = makeFont(kFont);
    const auto out = text::composeFont(font.data(), font.size());
    REQUIRE(!out.empty());
    // Tilde rows of 0xA4 (rows 0-1), centred on the 4-wide base: shift (4 - 5) / 2 = 0.
    CHECK(glyphRows(out, 0xC6) == std::vector<std::string>{ ".#.#", "#.#.", ".##.", "#.#.", ".###", "...." });
    CHECK(glyphRows(out, 0xE4) == std::vector<std::string>{ ".#.#", "#.#.", ".##.", "#..#", ".##.", "...." });
}

TEST_CASE("composeFont draws accented capitals as the plain capital and leaves other codes alone")
{
    const auto font = makeFont(kFont);
    const auto out = text::composeFont(font.data(), font.size());
    REQUIRE(!out.empty());
    for (const text::PlainCapital& p : text::kPlainCapitals)
        CHECK(glyphRows(out, p.code) == glyphRows(font, p.plain));
    for (const Glyph& g : kFont)
        CHECK(glyphRows(out, g.code) == glyphRows(font, g.code));
}

TEST_CASE("composeFont refuses fonts it cannot extend")
{
    const auto full = makeFont(kFont, 6, 255);           // no room left in an 8-bit stride
    CHECK(text::composeFont(full.data(), full.size()).empty());
    const auto font = makeFont(kFont);
    CHECK(text::composeFont(font.data(), 7).empty());   // shorter than the header
}

TEST_CASE("pickLanguageFont uses the composed font only for Portuguese")
{
    const char* original = "orig";
    const std::vector<unsigned char> composed = { 1, 2, 3 };
    CHECK(text::pickLanguageFont("PORTUGUE", original, composed) == (const char*)composed.data());
    CHECK(text::pickLanguageFont("ENGLISH", original, composed) == original);
    CHECK(text::pickLanguageFont("PORTUGUE", original, {}) == original);
}
