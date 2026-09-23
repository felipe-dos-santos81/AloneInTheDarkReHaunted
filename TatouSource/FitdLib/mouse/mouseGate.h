///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: "ignore clicks until the button is released".
// Engine-free: standard headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

namespace mouse
{

// Armed when a screen opens, so the press that opened it (a HUD icon, a click
// on an object) is not also that screen's first click.
class ClickGate
{
public:
    void arm() { armed_ = true; }
    bool armed() const { return armed_; }

    // Call once per frame with the frame's click edge and the button state.
    bool filter(bool clickedThisFrame, bool downNow)
    {
        if (armed_)
        {
            if (!downNow)
                armed_ = false;
            return false;
        }
        return clickedThisFrame;
    }

private:
    bool armed_ = false;
};

} // namespace mouse
