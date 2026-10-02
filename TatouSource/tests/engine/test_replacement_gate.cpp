///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: when a body is drawn as its HD replacement.
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "replacementGate.h"

using namespace models;

TEST_CASE("replacement gate: drawn only when every condition holds")
{
    const GateInput all{ true, true, true, true, false, true, true, true };
    CHECK(drawReplacement(all));
    bool GateInput::*const needed[] = { &GateInput::optionOn,  &GateInput::isAitd1,    &GateInput::callerAllows,
                                        &GateInput::infoAnim,  &GateInput::entryReady, &GateInput::poseOk,
                                        &GateInput::programReady };
    for (bool GateInput::*field : needed)
    {
        GateInput g = all;
        g.*field = false;
        CHECK_FALSE(drawReplacement(g));
    }
    GateInput optimised = all;
    optimised.infoOptimise = true;
    CHECK_FALSE(drawReplacement(optimised));
}
