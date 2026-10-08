///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models, compare mode: four frames read back from the scene
// snapshot (osystem_captureScenePreview) and written as PNG next to the saves,
// for tools/hd_compare.py.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "hdCompare.h"

#include <cstdio>
#include <string>
#include <vector>

#include "configRemaster.h"
#include "modelReplacement.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stbImageWrite.h"

extern "C" {
    extern char homePath[512];
}

namespace
{
struct Shot
{
    const char* file;
    bool hdModels; // replacements drawn in this frame (false: hdCompareForcesClassic)
    bool unlit;
    bool hidden;
};
const Shot kShots[] = { { "hdcompare_lit.png", true, false, false },
                        { "hdcompare_unlit.png", true, true, false },
                        { "hdcompare_hidden.png", true, false, true },
                        { "hdcompare_classic.png", false, false, false } };
constexpr int kShotCount = 4;
constexpr int kSettleFrames = 120; // after the first replacement loaded: let the room settle
constexpr int kReadbackFrames = 3; // bgfx::readTexture's data is ready two frames after it is asked

struct Draw
{
    std::string key;
    int box[4];
};

int s_shot = -1;       // -1: waiting; kShotCount: done
int s_frames = 0;
bool s_capturing = false; // the snapshot of the current shot is being read back
bool s_unlit = false;
bool s_hidden = false;
bool s_classic = false;
bool s_holdTime = false;
std::vector<Draw> s_draws, s_unlitDraws;

void writeShot(const Shot& shot)
{
    osystem_finalizeScenePreview();
    const unsigned char* rgba = osystem_getScenePreviewData();
    const int w = osystem_getScenePreviewWidth(), h = osystem_getScenePreviewHeight();
    const std::string path = std::string(homePath) + shot.file;
    const bool ok = rgba && w > 0 && h > 0 && stbi_write_png(path.c_str(), w, h, 4, rgba, w * 4);
    osystem_releaseScenePreview();
    printf("HDCOMPARE %s %s\n", path.c_str(), ok ? "written" : "NOT written");
    fflush(stdout);
}

void writeDraws()
{
    const std::string path = std::string(homePath) + "hdcompare.txt";
    FILE* f = fopen(path.c_str(), "w");
    if (!f)
        return;
    fprintf(f, "# key x0 y0 x1 y1 (320x200 box of each replacement drawn in hdcompare_unlit.png)\n");
    for (const Draw& d : s_unlitDraws)
        fprintf(f, "%s %d %d %d %d\n", d.key.c_str(), d.box[0], d.box[1], d.box[2], d.box[3]);
    fclose(f);
}
}

bool hdCompareUnlit()
{
    return s_unlit;
}

bool hdCompareHidden()
{
    return s_hidden;
}

bool hdCompareForcesClassic()
{
    return s_classic;
}

bool hdCompareHoldsTime()
{
    return s_holdTime;
}

void hdCompareNoteDraw(const char* key, const int box[4])
{
    if (g_remasterConfig.debug.hdModelsCompare)
        s_draws.push_back(Draw{ key, { box[0], box[1], box[2], box[3] } });
}

void hdCompareEndFrame()
{
    std::vector<Draw> frameDraws;
    frameDraws.swap(s_draws); // what this frame drew
    if (!g_remasterConfig.debug.hdModelsCompare || s_shot >= kShotCount)
        return;
    if (s_shot < 0)
    {
        if (modelReplacementLoads() == 0 || !g_remasterConfig.graphics.enableHDModels || ++s_frames < kSettleFrames)
            return;
        if (!s_holdTime)
        {
            s_holdTime = true; // stop the clock; the next frame is shot 0 (HD, lit)
            return;
        }
        s_shot = 0;
    }
    const Shot& shot = kShots[s_shot];
    if (!s_capturing)
    {
        osystem_captureScenePreview(); // the frame just presented
        if (shot.unlit)
            s_unlitDraws = frameDraws;
        s_capturing = true;
        s_frames = 0;
        return;
    }
    if (++s_frames < kReadbackFrames)
        return;
    writeShot(shot);
    s_capturing = false;
    if (++s_shot < kShotCount)
    {
        s_classic = !kShots[s_shot].hdModels; // the next frame draws as it wants
        s_unlit = kShots[s_shot].unlit;
        s_hidden = kShots[s_shot].hidden;
    }
    else
    {
        writeDraws();
        s_classic = false;
        s_unlit = false;
        s_hidden = false;
        s_holdTime = false;
    }
}
