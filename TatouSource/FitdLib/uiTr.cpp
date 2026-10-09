///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// tr()/trDos(): the uiText table bound to the selected language.
///////////////////////////////////////////////////////////////////////////////
#include "uiTr.h"
#include "text/uiText.h"

#include <string>

extern std::string languageNameString;

const char* tr(const char* english)
{
    return languageNameString == "PORTUGUE" ? text::uiText(languageNameString, english) : english;
}

const char* trDos(const char* english) { return text::uiTextDos(languageNameString, english); }
