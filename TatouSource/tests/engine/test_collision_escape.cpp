///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the way out of a box an actor already overlaps.
///////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "collisionEscape.h"

using namespace physics;

TEST_CASE("escapeStep keeps a step that does not press deeper into the box")
{
    // The bedroom window creature (world object 57) left by its entry track
    // inside the broken pane (world object 51): it spans the pane's depth, so
    // walking into the room leaves the overlap as it is.
    int stepX = 0;
    int stepZ = -50;
    escapeStep(Span{ -1107, -393 }, Span{ 1802, 2516 }, Span{ -1250, -250 }, Span{ 2090, 2190 }, &stepX, &stepZ);
    CHECK(stepX == 0);
    CHECK(stepZ == -50);
}

TEST_CASE("escapeStep drops only the component that presses deeper in")
{
    // Overlapping the box's low x edge by 50 and its high z edge by 50.
    int stepX = 20; // deeper: 70
    int stepZ = 20; // shallower: 30
    escapeStep(Span{ 100, 300 }, Span{ 100, 300 }, Span{ 250, 400 }, Span{ 0, 150 }, &stepX, &stepZ);
    CHECK(stepX == 0);
    CHECK(stepZ == 20);

    stepX = -20; // shallower
    stepZ = -20; // deeper
    escapeStep(Span{ 100, 300 }, Span{ 100, 300 }, Span{ 250, 400 }, Span{ 0, 150 }, &stepX, &stepZ);
    CHECK(stepX == -20);
    CHECK(stepZ == 0);
}

TEST_CASE("escapeStep lets an actor walk straight out of a box it is inside")
{
    int stepX = 30;
    int stepZ = 0;
    escapeStep(Span{ 100, 200 }, Span{ 100, 200 }, Span{ 0, 1000 }, Span{ 0, 1000 }, &stepX, &stepZ);
    CHECK(stepX == 30); // the overlap stays the actor's own width until it reaches the edge
    CHECK(stepZ == 0);
}
