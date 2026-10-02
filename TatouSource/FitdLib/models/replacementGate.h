///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: when a body is drawn as its HD replacement. Every
// condition must hold; any one failing draws the classic body. Engine-free:
// standard headers only (none needed).
///////////////////////////////////////////////////////////////////////////////
#pragma once

namespace models
{

struct GateInput
{
    bool optionOn;     // graphics.hdModels
    bool isAitd1;      // AITD2/3 and JACK stay classic
    bool callerAllows; // only the world draws (AllRedraw, drawSceneObjects) ask for it
    bool infoAnim;     // the body has bone groups
    bool infoOptimise; // AITD2+ pose math: never replaced
    bool entryReady;   // a valid .hdm for this body is loaded
    bool poseOk;       // poseGroups produced matrices this frame
};

inline bool drawReplacement(const GateInput& g)
{
    return g.optionOn && g.isAitd1 && g.callerAllows && g.infoAnim && !g.infoOptimise && g.entryReady && g.poseOk;
}

} // namespace models
