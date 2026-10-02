///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models, compare mode (debug.hdModelsCompare): a developer
// check that the HD path draws where the classic one does. Off by default;
// with it off nothing here runs.
///////////////////////////////////////////////////////////////////////////////
#pragma once

// After bgfx::frame(): once a replacement has loaded and the room has settled
// (kSettleFrames), writes four consecutive frames next to the saves —
// hdcompare_lit.png (HD as played), hdcompare_unlit.png (HD showing its bare
// texture), hdcompare_hidden.png (the replaced bodies not drawn at all) and
// hdcompare_classic.png (replacements forced off for that frame) — with
// hdcompare.txt listing each replacement drawn in the unlit frame and its
// screen box. Once per run. tools/hd_compare.py scores them.
void hdCompareEndFrame();

// The replacement shader shows the bare texture (the unlit frame).
bool hdCompareUnlit();

// A body with a replacement is not drawn at all (the hidden frame).
bool hdCompareHidden();

// The classic frame: replacements are off for this frame, without touching
// graphics.hdModels (a config save in the meantime must not record it off).
bool hdCompareForcesClassic();

// The game clock stands still (process_events) from one frame before the
// first shot until the last, so every shot shows the same pose.
bool hdCompareHoldsTime();

// drawModelReplacement reports each replacement it draws, with its 320x200 box.
void hdCompareNoteDraw(const char* key, const int box[4]);
