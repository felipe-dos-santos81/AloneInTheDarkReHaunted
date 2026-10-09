#include "uiTr.h"
#include "text/uiText.h"

#include <string>

extern std::string languageNameString;

const char* tr(const char* english) { return text::uiText(languageNameString, english); }
const char* trDos(const char* english) { return text::uiTextDos(languageNameString, english); }
