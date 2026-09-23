#include "doctest.h"
#include "mouseTypes.h"

using mouse::Point;

TEST_CASE("windowToLogical stretches the whole window onto 320x200")
{
    Point p;
    REQUIRE(mouse::windowToLogical(0.0f, 0.0f, 1280, 800, &p));
    CHECK(p == Point{ 0, 0 });
    REQUIRE(mouse::windowToLogical(640.0f, 400.0f, 1280, 800, &p));
    CHECK(p == Point{ 160, 100 });
    REQUIRE(mouse::windowToLogical(1279.9f, 799.9f, 1280, 800, &p));
    CHECK(p == Point{ 319, 199 });
}

TEST_CASE("windowToLogical handles non-integer scales and odd window sizes")
{
    Point p;
    REQUIRE(mouse::windowToLogical(683.0f, 384.0f, 1366, 768, &p));
    CHECK(p == Point{ 160, 100 });
    REQUIRE(mouse::windowToLogical(1365.99f, 767.99f, 1366, 768, &p));
    CHECK(p == Point{ 319, 199 });
    REQUIRE(mouse::windowToLogical(0.5f, 0.5f, 321, 201, &p));
    CHECK(p == Point{ 0, 0 });
}

TEST_CASE("windowToLogical reports positions outside the window as nothing")
{
    Point p{ 7, 7 };
    CHECK_FALSE(mouse::windowToLogical(-1.0f, 10.0f, 1280, 800, &p));
    CHECK_FALSE(mouse::windowToLogical(10.0f, -0.5f, 1280, 800, &p));
    CHECK_FALSE(mouse::windowToLogical(1280.0f, 10.0f, 1280, 800, &p));
    CHECK_FALSE(mouse::windowToLogical(10.0f, 800.0f, 1280, 800, &p));
    CHECK_FALSE(mouse::windowToLogical(10.0f, 10.0f, 0, 800, &p));
}

using mouse::Event;
using mouse::EventType;
using mouse::Frame;
using mouse::FrameQueue;

TEST_CASE("a press and release inside one frame both reach the game, in order")
{
    FrameQueue q;
    q.push(Event{ EventType::Down, true, Point{ 10, 20 }, 1 });
    q.push(Event{ EventType::Up, true, Point{ 10, 20 }, 1 });
    q.publish(true, Point{ 10, 20 }, false, false);
    Frame f;
    REQUIRE(q.take(&f));
    REQUIRE(f.events.size() == 2);
    CHECK(f.events[0].type == EventType::Down);
    CHECK(f.events[1].type == EventType::Up);
    CHECK_FALSE(f.leftDown);
}

TEST_CASE("an unconsumed frame is replaced by the next one and a frame is taken once")
{
    FrameQueue q;
    q.push(Event{ EventType::Down, true, Point{ 1, 1 }, 1 });
    q.publish(true, Point{ 1, 1 }, true, false);
    q.push(Event{ EventType::Motion, true, Point{ 2, 2 }, 0 });
    q.publish(true, Point{ 2, 2 }, true, true);
    Frame f;
    REQUIRE(q.take(&f));
    REQUIRE(f.events.size() == 1);
    CHECK(f.events[0].type == EventType::Motion);
    CHECK(f.blocked);
    CHECK_FALSE(q.take(&f));
}

TEST_CASE("events pushed after a publish belong to the next frame")
{
    FrameQueue q;
    q.publish(false, Point{}, false, false);
    q.push(Event{ EventType::FocusLost, false, Point{}, 0 });
    Frame f;
    REQUIRE(q.take(&f));
    CHECK(f.events.empty());
    q.publish(false, Point{}, false, false);
    REQUIRE(q.take(&f));
    REQUIRE(f.events.size() == 1);
    CHECK(f.events[0].type == EventType::FocusLost);
}
