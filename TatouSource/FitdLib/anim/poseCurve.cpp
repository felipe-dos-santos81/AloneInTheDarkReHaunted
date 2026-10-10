///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// See poseCurve.h.
///////////////////////////////////////////////////////////////////////////////
#include "poseCurve.h"

#include <cmath>

namespace anim
{

Neighbours neighbours(int startIndex, int target, int numFrames, bool loops)
{
    Neighbours n{ kReuse, kReuse };
    if (startIndex >= 0 && startIndex < numFrames)
    {
        if (startIndex > 0)
            n.before = startIndex - 1;
        else if (loops && numFrames > 1)
            n.before = numFrames - 1;
    }
    if (target + 1 < numFrames)
        n.after = target + 1;
    else if (loops && numFrames > 1)
        n.after = 0;
    return n;
}

namespace
{

// A neighbour's slope contribution: with no neighbour (its segment 0 ticks
// long) the slope is the segment's own, so the curve leaves or meets it linearly.
double slope(double from, double to, double fromTicks, double toTicks)
{
    return (to - from) / (fromTicks + toTicks);
}

int offset(double p0, double p1, double p2, double p3, int before, int length, int after, int elapsed, float strength)
{
    if (length <= 0 || strength <= 0.0f)
        return 0;
    const double t = (double)elapsed / length;
    const double m1 = (before > 0 ? slope(p0, p2, before, length) : (p2 - p1) / length) * length;
    const double m2 = (after > 0 ? slope(p1, p3, length, after) : (p2 - p1) / length) * length;
    const double t2 = t * t, t3 = t2 * t;
    const double curve = (2 * t3 - 3 * t2 + 1) * p1 + (t3 - 2 * t2 + t) * m1 + (-2 * t3 + 3 * t2) * p2 + (t3 - t2) * m2;
    const double linear = p1 + (p2 - p1) * t;
    const double s = strength > 1.0f ? 1.0 : strength;
    return (int)std::lround(s * (curve - linear));
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

int curveOffset(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed, float strength)
{
    return offset(p0, p1, p2, p3, before, length, after, elapsed, strength);
}

int angleCurveOffset(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed, float strength)
{
    const int q2 = nearAngle(p2, p1);
    return offset(nearAngle(p0, p1), p1, q2, nearAngle(p3, q2), before, length, after, elapsed, strength);
}

} // namespace anim
