///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the curve through an animation's keyframes (HD display).
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "poseCurve.h"

#include <cstdlib>

using anim::kReuse;

namespace
{
// The displayed value: the engine's linear value plus the curve's offset.
double shown(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed)
{
    return p1 + (double)(p2 - p1) * elapsed / length + anim::curveOffset(p0, p1, p2, p3, before, length, after, elapsed, 1.0f);
}
}

TEST_CASE("the curve meets the linear pose at both keyframes and at strength 0")
{
    for (int elapsed : { 0, 10 })
        CHECK(anim::curveOffset(-300, 500, 2000, 900, 7, 10, 25, elapsed, 1.0f) == 0);
    CHECK(anim::curveOffset(-300, 500, 2000, 900, 7, 10, 25, 4, 0.0f) == 0);
    CHECK(anim::curveOffset(-300, 500, 2000, 900, 7, 10, 25, 4, 1.0f) != 0);
}

TEST_CASE("the speed is the same on both sides of a keyframe, whatever the segments' lengths")
{
    // keyframes 0, 4000, 6000, 12000, 13000 reached after 100, 300 and 200 ticks: a uniform
    // Catmull-Rom curve would change speed at 6000, where a 100-tick segment meets a 300-tick one
    const double endOfFirst = shown(0, 4000, 6000, 12000, 100, 100, 300, 100) - shown(0, 4000, 6000, 12000, 100, 100, 300, 99);
    const double startOfNext = shown(4000, 6000, 12000, 13000, 100, 300, 200, 1) - shown(4000, 6000, 12000, 13000, 100, 300, 200, 0);
    CHECK(endOfFirst == doctest::Approx(startOfNext).epsilon(0.02)); // both near (12000 - 4000) / 400 = 20 per tick
}

TEST_CASE("without a neighbour the curve leaves and meets its segment's own slope")
{
    // a snapshot start (no keyframe before) and a one-shot end (none after): linear throughout
    for (int elapsed = 0; elapsed <= 12; ++elapsed)
        CHECK(anim::curveOffset(500, 500, 2000, 2000, 0, 12, 0, elapsed, 1.0f) == 0);
}

TEST_CASE("an angle takes the short way round, as PatchInterAngle does")
{
    // 1000 -> 20 is 44 steps forward across 1023/0; 980 before and 60 after continue the same way
    const int off = anim::angleCurveOffset(980, 1000, 20, 60, 10, 10, 10, 5, 1.0f);
    CHECK(std::abs(off) <= 2);
    CHECK(off == anim::curveOffset(980, 1000, 20 + 1024, 60 + 1024, 10, 10, 10, 5, 1.0f));
}

TEST_CASE("neighbours: within an animation, after an animation change, around a loop and at a one-shot end")
{
    struct Row { int start, target, frames; bool loops; int before, after; };
    const Row rows[] = {
        { 2, 3, 6, false, 1, 4 },            // inside the animation
        { kReuse, 0, 6, false, kReuse, 1 },  // the start is a snapshot of the previous animation's pose
        { 5, 0, 6, true, 4, 1 },             // a loop wrapping from the last keyframe to the first
        { 0, 1, 6, true, 5, 2 },             // the keyframe before the first is the last
        { 4, 5, 6, false, 3, kReuse },       // a one-shot animation's end
        { 4, 5, 6, true, 3, 0 },             // a looping animation's end
    };
    for (const Row& r : rows)
    {
        const anim::Neighbours n = anim::neighbours(r.start, r.target, r.frames, r.loops);
        CHECK(n.before == r.before);
        CHECK(n.after == r.after);
    }
}
