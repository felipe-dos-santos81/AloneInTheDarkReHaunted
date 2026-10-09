///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// The remaster's own menu strings per language. Engine-free.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <string>

namespace text {

// One English string and its translations, UTF-8; empty or null falls back to English.
struct UiRow { const char* en; const char* fr; const char* it; const char* es; const char* de; const char* pt; };
extern const UiRow kUiRows[];
extern const size_t kUiRowCount;

// `english` in `lang` (a languageNameString: FRANCAIS, ITALIANO, ESPAGNOL, DEUTSCH,
// PORTUGUE), UTF-8, for ImGui. Any other language, or a missing row, gives `english`.
const char* uiText(const std::string& lang, const char* english);

// The same, in the game's codepage (CP850, 0xA9 = (c)), for the bitmap font and
// the TrueType queue. Converted once per string and kept.
const char* uiTextDos(const std::string& lang, const char* english);

// UTF-8 -> CP850 with the font's 0xA9 = (c); false if a character has no code.
bool toDos(const char* utf8, std::string* out);

} // namespace text
