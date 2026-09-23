///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Copyright (C) 2026 Infogrames / Spacefarer Retro Remasters LLC
// Based on FITD by yaz0r, Re-haunted is released under GPL
// Author: Jake Jackson (jake@spacefarergames.com)
//
// Implementations of non-inline helpers declared in nativeLifeHelpers.h
///////////////////////////////////////////////////////////////////////////////

#include "nativeLifeHelpers.h"
#include "gameTime.h"
#include "pak.h"
#include "hdBackground.h"
#include "hdBackgroundRenderer.h"
#include "menuMouse.h"

extern bool g_currentBackgroundIsHD;
void recreateBackgroundTexture(int width, int height);

void life_Picture(int pictureIndex, int delay, int sampleId)
{
    SaveTimerAnim();

    HDBackgroundInfo* hdBg = loadHDBackground("ITD_RESS", pictureIndex);
    if (hdBg)
    {
        updateBackgroundTextureHD(hdBg->data, hdBg->width, hdBg->height, hdBg->channels);
        if (hdBg->isAnimated)
            setCurrentAnimatedHDBackground(hdBg);
        else
            freeHDBackground(hdBg);
    }
    else if (g_currentBackgroundIsHD)
    {
        recreateBackgroundTexture(320, 200);
    }

    LoadPak("ITD_RESS", pictureIndex, aux);
    FastCopyScreen(aux, frontBuffer);
    osystem_CopyBlockPhys((unsigned char*)frontBuffer, 0, 0, 320, 200);
    osystem_drawBackground();

    unsigned int chrono;
    startChrono(&chrono);
    playSound(sampleId);

    mouseWorldTakeOver(); // entered mid-frame from play (I2)
    do
    {
        process_events();
        if (menuMouseClicked())
            break;
        osystem_startFrame();
        osystem_drawBackground();
        osystem_stopFrame();
        osystem_flip(NULL);

        if (evalChrono(&chrono) > (unsigned int)delay)
            break;
    } while (!key && !Click);

    RestoreTimerAnim();

    setCurrentAnimatedHDBackground(nullptr);
    FlagInitView = 1;
}

void life_WaitGameOver()
{
    mouseWorldTakeOver(); // entered mid-frame from play (I2)
    while (key || JoyD || Click)
        process_events();
    while (!key && !JoyD && !Click)
    {
        process_events();
        if (menuMouseClicked())
            break;
    }
    FlagGameOver = 1;
}
