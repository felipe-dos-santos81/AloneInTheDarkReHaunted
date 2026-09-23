///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: screen <-> floor picking. Engine-free: standard headers only.
// The projection replicates the renderer's integer path exactly
// (renderer.cpp transformPoint + the actor-vertex divide) so picking can never
// drift from what is drawn.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "mouseTypes.h"

namespace mouse
{

// A room's world origin (roomDataStruct worldX/Y/Z).
struct RoomOrigin
{
    int worldX = 0;
    int worldY = 0;
    int worldZ = 0;
};

// A floor camera framed in one room's coordinate space.
struct Camera
{
    int alpha = 0;
    int beta = 0;
    int gamma = 0;
    int posX = 0;
    int posY = 0;
    int posZ = 0;
    int focal1 = 0; // cameraPerspective
    int focal2 = 0; // cameraFovX
    int focal3 = 0; // cameraFovY
    const int16_t* cosTable = nullptr;
};

// Same framing as InitView: (camX - worldX)*10, (worldY - camY)*10, (worldZ - camZ)*10.
Camera frameCamera(int alpha, int beta, int gamma, int camX, int camY, int camZ,
                   int focal1, int focal2, int focal3, RoomOrigin room, const int16_t* cosTable);

struct Vec2
{
    double x = 0.0;
    double y = 0.0;
};

// Room-frame point -> logical screen, or nothing when culled (height clamp,
// depth <= 50). Not clipped to the 320x200 frame.
std::optional<Vec2> projectPoint(const Camera& camera, int x, int y, int z);

// Row-major 3x3 projective map; m[8] normalised to 1.
struct Homography
{
    std::array<double, 9> m{};
};

std::optional<Homography> fitHomography(const std::array<Vec2, 4>& src, const std::array<Vec2, 4>& dst);
std::optional<Homography> invert(const Homography& h);
std::optional<Vec2> apply(const Homography& h, double x, double y);

// Re-frame a point from one room's origin to another's (FITD AdjustZV signs).
XZ reframe(XZ p, RoomOrigin from, RoomOrigin to);
int reframeY(int y, RoomOrigin from, RoomOrigin to);

} // namespace mouse
