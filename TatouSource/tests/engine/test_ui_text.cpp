///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the remaster menu string table (text/uiText).
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "uiText.h"

#include <cstring>
#include <regex>
#include <string>

TEST_CASE("uiText returns the language's text, else the English key")
{
    CHECK(std::string(text::uiText("FRANCAIS", "Controls")) == "Commandes");
    CHECK(std::string(text::uiText("PORTUGUE", "Controls")) == "Controles");
    CHECK(std::string(text::uiText("ENGLISH", "Controls")) == "Controls");
    CHECK(std::string(text::uiText("PORTUGUE", "Not in the table")) == "Not in the table");
}

TEST_CASE("uiTextDos converts UTF-8 to the game's codepage")
{
    CHECK(std::string(text::uiTextDos("FRANCAIS", "Hints: On")) == "Indices: Activ\x82");
    std::string out;
    CHECK(text::toDos("n\xC3\xA3o \xC2\xA9", &out));                 // não ©
    CHECK(out == "n\xC6o \xA9");
    CHECK_FALSE(text::toDos("\xE2\x80\x94", &out));                    // em dash: not CP850
}

TEST_CASE("every translation keeps its key's ImGui id suffix and printf specifiers, and encodes")
{
    const std::regex spec("%[-+ #0]*[0-9]*(\\.[0-9]+)?[a-zA-Z]");
    auto specs = [&](const char* s) {
        std::string all;
        for (std::cregex_iterator it(s, s + std::strlen(s), spec), end; it != end; ++it) all += it->str();
        return all;
    };
    auto suffix = [](const char* s) { const char* h = std::strstr(s, "##"); return std::string(h ? h : ""); };
    // The bitmap font draws these CP850 bytes: printable ASCII, then the accented letters it has.
    const std::string drawn = "\xA0\x82\xA1\xA2\xA3\x83\x88\x93\x85\x87\x81\x89\xC6\xE4"   // a e i o u a e o a c u e a o with accents
                              "\xB7\xB5\xB6\xC7\xD2\xD6\xE0\xE2\xE5\xE9\x90\x80\xA9\xFA";  // capitals, (c), middle dot
    for (size_t i = 0; i < text::kUiRowCount; i++)
    {
        const text::UiRow& r = text::kUiRows[i];
        for (const char* t : { r.fr, r.it, r.es, r.de, r.pt })
        {
            if (!t || !*t) continue;
            INFO(r.en << " -> " << t);
            CHECK(suffix(t) == suffix(r.en));
            CHECK(specs(t) == specs(r.en));
            std::string dos;
            CHECK(text::toDos(t, &dos));
        }
        if (r.pt && *r.pt)
        {
            std::string dos;
            text::toDos(r.pt, &dos);
            for (unsigned char c : dos)
            {
                INFO(r.en << " -> " << r.pt << " byte " << int(c));
                CHECK(((c >= 0x20 && c <= 0x7E) || drawn.find(char(c)) != std::string::npos));
            }
        }
    }
}
