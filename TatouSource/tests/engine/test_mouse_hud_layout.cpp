#include "doctest.h"
#include "mouseHudLayout.h"

using namespace mouse;

TEST_CASE("HUD icons are 20x16 at x = 4, 28, 52 and y = 4")
{
    auto r = hudIconRects();
    CHECK(r[0] == Rect{ 4, 4, 23, 19 });
    CHECK(r[1] == Rect{ 28, 4, 47, 19 });
    CHECK(r[2] == Rect{ 52, 4, 71, 19 });
}

TEST_CASE("HUD hit rects are padded by 2 and never overlap")
{
    auto h = hudIconHitRects();
    CHECK(h[0] == Rect{ 2, 2, 25, 21 });
    CHECK(h[1] == Rect{ 26, 2, 49, 21 });
    CHECK(h[2] == Rect{ 50, 2, 73, 21 });
    for (int i = 0; i + 1 < kHudIconCount; ++i)
        CHECK(h[i].x2 < h[i + 1].x1);
}

TEST_CASE("hudIconAt answers the padded rects")
{
    CHECK(hudIconAt(Point{ 24, 10 }) == HudIcon::Inventory);
    CHECK(hudIconAt(Point{ 26, 10 }) == HudIcon::Map);
    CHECK(hudIconAt(Point{ 73, 21 }) == HudIcon::Menu);
    CHECK_FALSE(hudIconAt(Point{ 100, 10 }).has_value());
    CHECK_FALSE(hudIconAt(Point{ 10, 22 }).has_value());
}

TEST_CASE("forgivingBox pads, grows to 12x12 centred, and slides inside the frame")
{
    CHECK(forgivingBox(Rect{ 100, 100, 101, 101 }) == Rect{ 95, 95, 106, 106 });
    CHECK(forgivingBox(Rect{ 0, 0, 3, 3 }) == Rect{ 0, 0, 11, 11 });
    CHECK(forgivingBox(Rect{ 316, 196, 319, 199 }) == Rect{ 308, 188, 319, 199 });
    CHECK(forgivingBox(Rect{ 50, 60, 90, 120 }) == Rect{ 48, 58, 92, 122 });
}
