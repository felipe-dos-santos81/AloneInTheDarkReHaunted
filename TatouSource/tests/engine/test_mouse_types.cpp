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
