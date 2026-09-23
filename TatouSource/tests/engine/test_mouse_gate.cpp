#include "doctest.h"
#include "mouseGate.h"

TEST_CASE("an unarmed gate passes click edges through")
{
    mouse::ClickGate gate;
    CHECK(gate.filter(true, true));
    CHECK_FALSE(gate.filter(false, true));
    CHECK_FALSE(gate.filter(false, false));
}

TEST_CASE("an armed gate swallows clicks until the button has been released")
{
    mouse::ClickGate gate;
    gate.arm();
    CHECK(gate.armed());
    CHECK_FALSE(gate.filter(true, true));   // the press that opened the screen
    CHECK_FALSE(gate.filter(false, true));  // still held
    CHECK_FALSE(gate.filter(false, false)); // released: disarms, this frame still swallowed
    CHECK_FALSE(gate.armed());
    CHECK(gate.filter(true, true));         // the next real click passes
}

TEST_CASE("arming while the button is already up disarms on the next frame")
{
    mouse::ClickGate gate;
    gate.arm();
    CHECK_FALSE(gate.filter(false, false));
    CHECK_FALSE(gate.armed());
    CHECK(gate.filter(true, true));
}
