///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine adapter.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "mouseWorld.h"
#include "mouseGate.h"

namespace
{
mouse::ClickGate s_screenGate;
bool s_worldActive = false;
}

void mouseWorldTakeOver()
{
    s_worldActive = false;
    s_screenGate.arm();
}

bool mouseWorldIsActive()
{
    return s_worldActive;
}

bool mouseScreenClickFilter(bool clickedThisFrame, bool downNow)
{
    return s_screenGate.filter(clickedThisFrame, downNow);
}
