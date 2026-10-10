///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: a smooth curve through an animation's keyframes, for
// the displayed pose only. The engine interpolates each group linearly from
// the segment's start to its target keyframe; the curve adds an offset to that
// value so the motion keeps its speed through every keyframe (a timed cubic
// Hermite curve: each keyframe's slope is the change from the keyframe before
// it to the one after it over the time between them). The offset is zero at
// both keyframes and at strength 0, so the curve meets the linear pose there.
// Engine-free: standard headers only. The engine adapter is anim.cpp.
///////////////////////////////////////////////////////////////////////////////
#pragma once

namespace anim
{

constexpr int kNoKeyframe = -1; // no neighbour keyframe: the segment's own end stands in for it

// The keyframes around the segment that runs from `startIndex` (the start
// keyframe's index in the animation, or kNoKeyframe when the start is not one of
// its keyframes: a snapshot of the pose at an animation change) to `target`, in
// an animation of `numFrames` keyframes that loops or not. Only a start that is
// the keyframe before the target has a keyframe before it; a hold (the start is
// the target) has no neighbours, so it stays still.
struct Neighbours
{
    int before; // the keyframe before the start, or kNoKeyframe
    int after;  // the keyframe after the target, or kNoKeyframe
};
Neighbours neighbours(int startIndex, int target, int numFrames, bool loops);

// The offset to add to the engine's linear value between p1 (the start) and
// p2 (the target) after `elapsed` of the segment's `length` ticks. p0 and p3
// are the neighbours, `before` and `after` the lengths of the segments that
// join them (0 when there is no neighbour: the slope there is the segment's
// own). strength 0..1 blends linear (0) to the full curve (1).
int curveOffset(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed, float strength);

// The same for a 10-bit angle: p0, p2 and p3 are first brought within half a
// turn (512) of p1, the way the engine's PatchInterAngle takes the short way.
int angleCurveOffset(int p0, int p1, int p2, int p3, int before, int length, int after, int elapsed, float strength);

} // namespace anim
