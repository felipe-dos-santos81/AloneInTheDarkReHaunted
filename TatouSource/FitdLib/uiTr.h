///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// The remaster menus' strings in the selected language (text/uiText.h).
///////////////////////////////////////////////////////////////////////////////
#pragma once

// UTF-8, for the ImGui options dialog. Translated into Portuguese only: the
// other languages' cells belong to the bitmap menus, and F1 stays English there.
const char* tr(const char* english);
// The game's codepage, for the bitmap font. Game thread only (unlocked cache).
const char* trDos(const char* english);
