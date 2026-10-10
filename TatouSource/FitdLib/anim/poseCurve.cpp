///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// See poseCurve.h.
///////////////////////////////////////////////////////////////////////////////
#include "poseCurve.h"

#include <cmath>

namespace anim
{

namespace
{

// The keyframe before / after `i` in an animation of `numFrames` keyframes:
// past either end, the other end when it loops, else none.
int previous(int i, int numFrames, bool loops)
{
    return i > 0 ? i - 1 : (loops && numFrames > 1 ? numFrames - 1 : kNoKeyframe);
}

int next(int i, int numFrames, bool loops)
{
    return i + 1 < numFrames ? i + 1 : (loops && numFrames > 1 ? 0 : kNoKeyframe);
}

// A neighbour's slope contribution: with no neighbour (its segment 0 ticks
// long) the slope is the segment's own, so the curve leaves or meets it linearly.
double slope(double from, double to, double fromTicks, double toTicks)
{
    return (to - from) / (fromTicks + toTicks);
}

// v brought within half a turn of `around`.
int nearAngle(int v, int around)
{
    int d = v - around;
    while (d > 0x200)
        d -= 0x400;
    while (d < -0x200)
        d += 0x400;
    return around + d;
}

} // namespace

Neighbours neighbours(int startIndex, int target, int numFrames, bool loops)
{
    // a hold (the start is the target: a reset or a fresh animation) stays still
    if (startIndex == target)
        return { kNoKeyframe, kNoKeyframe };
    // only a start that is the target's own previous keyframe has one before it
    const bool follows = startIndex >= 0 && startIndex == previous(target, numFrames, loops);
    return { follows ? previous(startIndex, numFrames, loops) : kNoKeyframe, next(target, numFrames, loops) };
}

int curveOffset(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed, float strength)
{
    if (length <= 0 || strength <= 0.0f)
        return 0;
    const double t = (double)elapsed / length;
    const double m1 = before > 0 ? slope(p0, p2, before, length) * length : p2 - p1;
    const double m2 = after > 0 ? slope(p1, p3, length, after) * length : p2 - p1;
    const double t2 = t * t, t3 = t2 * t;
    const double curve = (2 * t3 - 3 * t2 + 1) * p1 + (t3 - 2 * t2 + t) * m1 + (-2 * t3 + 3 * t2) * p2 + (t3 - t2) * m2;
    const double linear = p1 + (p2 - p1) * t;
    const double s = strength > 1.0f ? 1.0 : strength;
    return (int)std::lround(s * (curve - linear));
}

int angleCurveOffset(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed, float strength)
{
    const int q2 = nearAngle(p2, p1);
    return curveOffset(nearAngle(p0, p1), p1, q2, nearAngle(p3, q2), before, length, after, elapsed, strength);
}

} // namespace anim
