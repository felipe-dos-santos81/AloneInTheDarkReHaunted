///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Collision: the way out of a box an actor already overlaps. Engine-free
// (standard headers only), unit-tested in tests/engine.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <algorithm>

namespace physics
{

// One axis of a box, lo <= hi.
struct Span
{
    int lo = 0;
    int hi = 0;
};

// How far two spans overlap; zero or less when they are apart.
inline int overlap(Span a, Span b)
{
    return std::min(a.hi, b.hi) - std::max(a.lo, b.lo);
}

// The step an actor whose box (oldX, oldZ) already overlaps a blocker (fixX,
// fixZ) may still take: each component that does not press it deeper in.
// FITD's GereCollision zeroes the whole step there, a trap no actor leaves.
// AITD1's own data walks actors into it: an entry track turns collision off,
// walks in, and turns it back on wherever TL_GOTO's 400-unit "close enough"
// left it (the bedroom window creature, inside the broken pane). Ported from
// m-aitd (_escape_step).
inline void escapeStep(Span oldX, Span oldZ, Span fixX, Span fixZ, int* stepX, int* stepZ)
{
    if (overlap(Span{ oldX.lo + *stepX, oldX.hi + *stepX }, fixX) > overlap(oldX, fixX))
        *stepX = 0;
    if (overlap(Span{ oldZ.lo + *stepZ, oldZ.hi + *stepZ }, fixZ) > overlap(oldZ, fixZ))
        *stepZ = 0;
}

} // namespace physics
