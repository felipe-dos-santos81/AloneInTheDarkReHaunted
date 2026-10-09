///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Extends the AITD1 bitmap font for Portuguese.
///////////////////////////////////////////////////////////////////////////////
#include "fontCompose.h"

#include <cstring>

namespace text {

namespace {

struct Layout { int align, height, stride, table; bool wordStride; };

bool readLayout(const unsigned char* f, size_t size, Layout* l)
{
    if (size < 8)
        return false;
    l->align = f[0];
    l->height = f[2];
    // Like font.cpp SetFont: the stride is byte 3, or the little-endian s16 at byte 4 when byte 3 is 0.
    l->wordStride = f[3] == 0;
    l->stride = l->wordStride ? f[4] | (f[5] << 8) : f[3];
    l->table = (f[6] << 8) | f[7];
    // The strip runs from byte 8 to the table, which holds a u16 per code from align.
    // The game font's table stops at code 0xFE, 2 bytes short of 0xFF: allow that.
    return l->stride != 0 && l->height != 0 && l->table == 8 + l->height * l->stride
        && (size_t)l->table + (256 - l->align) * 2 <= size + 2;
}

int entryAt(const std::vector<unsigned char>& f, const Layout& l, int code)
{
    const size_t e = l.table + (size_t)(code - l.align) * 2;
    return e + 1 < f.size() ? (f[e] << 8) | f[e + 1] : 0;
}

void setEntry(std::vector<unsigned char>& f, const Layout& l, int code, int value)
{
    const size_t e = l.table + (size_t)(code - l.align) * 2;
    if (e + 1 < f.size()) { f[e] = (unsigned char)(value >> 8); f[e + 1] = (unsigned char)(value & 0xFF); }
}

bool pixel(const std::vector<unsigned char>& f, const Layout& l, int row, int bit)
{
    return (f[8 + row * l.stride + (bit >> 3)] & (0x80 >> (bit & 7))) != 0;
}

void setPixel(std::vector<unsigned char>& f, const Layout& l, int row, int bit)
{
    f[8 + row * l.stride + (bit >> 3)] |= (unsigned char)(0x80 >> (bit & 7));
}

bool rowBlank(const std::vector<unsigned char>& f, const Layout& l, int row, int bit, int width)
{
    for (int x = 0; x < width; x++)
        if (pixel(f, l, row, bit + x))
            return false;
    return true;
}

} // namespace

std::vector<unsigned char> composeFont(const unsigned char* font, size_t size)
{
    Layout in;
    if (!font || !readLayout(font, size, &in))
        return {};
    const std::vector<unsigned char> src(font, font + size);

    int added = 0;
    for (const ComposedGlyph& g : kComposed)
        added += entryAt(src, in, g.base) >> 12;
    Layout out = in;
    out.stride = in.stride + (added + 7) / 8;
    // Glyph entries hold a 12-bit starting bit: the new glyphs must end within bit 4096.
    if (in.stride * 8 + added > 4096)
        return {};
    // Keep the header form of the font; a byte stride that outgrows 255 moves to the s16.
    out.wordStride = in.wordStride || out.stride > 255;
    out.table = 8 + out.height * out.stride;

    // Header, the strip re-strided row by row, then the code table.
    std::vector<unsigned char> dst(out.table + (size - in.table), 0);
    std::memcpy(dst.data(), src.data(), 8);
    dst[3] = out.wordStride ? 0 : (unsigned char)out.stride;
    if (out.wordStride)
    {
        dst[4] = (unsigned char)(out.stride & 0xFF);
        dst[5] = (unsigned char)(out.stride >> 8);
    }
    dst[6] = (unsigned char)(out.table >> 8);
    dst[7] = (unsigned char)(out.table & 0xFF);
    for (int r = 0; r < in.height; r++)
        std::memcpy(&dst[8 + r * out.stride], &src[8 + r * in.stride], in.stride);
    std::memcpy(&dst[out.table], &src[in.table], size - in.table);

    int bit = in.stride * 8;   // new glyphs go after the original strip
    for (const ComposedGlyph& g : kComposed)
    {
        const int base = entryAt(src, in, g.base), donor = entryAt(src, in, g.markDonor);
        const int bw = base >> 12, bb = base & 0xFFF, dw = donor >> 12, db = donor & 0xFFF;
        // The mark: the donor's rows from its first inked row to its first blank row after it.
        int top = 0;
        while (top < in.height && rowBlank(src, in, top, db, dw))
            top++;
        int markEnd = top;
        while (markEnd < in.height && !rowBlank(src, in, markEnd, db, dw))
            markEnd++;
        const int shift = (bw - dw) / 2;
        for (int r = 0; r < in.height; r++)
            for (int x = 0; x < bw; x++)
            {
                const int dx = x - shift;
                const bool mark = r >= top && r < markEnd && dx >= 0 && dx < dw && pixel(src, in, r, db + dx);
                if (pixel(src, in, r, bb + x) || mark)
                    setPixel(dst, out, r, bit + x);
            }
        setEntry(dst, out, g.code, (bw << 12) | bit);
        bit += bw;
    }
    for (const PlainCapital& p : kPlainCapitals)
        setEntry(dst, out, p.code, entryAt(src, in, p.plain));
    return dst;
}

const char* pickLanguageFont(const std::string& lang, const char* original,
                             const std::vector<unsigned char>& composed)
{
    return lang == "PORTUGUE" && !composed.empty() ? (const char*)composed.data() : original;
}

} // namespace text
