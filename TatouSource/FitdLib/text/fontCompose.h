// TatouSource/FitdLib/text/fontCompose.h
///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Extends the AITD1 bitmap font for Portuguese: composes the glyphs it lacks
// from glyphs it has. Engine-free (standard headers only).
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace text {

// A glyph built from a base letter and the mark rows of a donor glyph.
struct ComposedGlyph { unsigned char code, base, markDonor; };
// ã and õ (CP850 0xC6, 0xE4): a / o with the tilde of ñ (0xA4).
// tools/aitd_data/lang.py COMPOSED lists the same codes.
extern const ComposedGlyph kComposed[2];

// Accented capitals the font has no room to mark: drawn as the plain capital.
// tools/aitd_data/lang.py PLAIN_CAPITALS lists the same codes.
struct PlainCapital { unsigned char code, plain; };
extern const PlainCapital kPlainCapitals[10];

// A copy of `font` (ITD_RESS entry 5, the layout font.cpp SetFont reads) with
// kComposed appended to the glyph strip and kPlainCapitals pointing at their
// plain capital; every other code is unchanged. Empty when the font is
// malformed or the widened stride would not fit its 8-bit header byte.
std::vector<unsigned char> composeFont(const unsigned char* font, size_t size);

// The font to draw `lang` with: the composed copy for Portuguese when there is one.
const char* pickLanguageFont(const std::string& lang, const char* original,
                             const std::vector<unsigned char>& composed);

} // namespace text
