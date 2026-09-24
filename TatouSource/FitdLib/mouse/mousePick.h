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

#include "mousePoly.h"
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

// A floor camera as the floor data stores it (cameraDataStruct).
struct CameraData
{
    int alpha = 0;
    int beta = 0;
    int gamma = 0;
    int x = 0;
    int y = 0;
    int z = 0;
    int focal1 = 0;
    int focal2 = 0;
    int focal3 = 0;
};

// Same framing as InitView: (camX - worldX)*10, (worldY - camY)*10, (worldZ - camZ)*10.
Camera frameCamera(const CameraData& cam, RoomOrigin room, const int16_t* cosTable);

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

// How far (logical px) a recovered floor point may reproject from its pixel.
constexpr int kReprojectPx = 2;
// How far (logical px) a snapped walk may land from the pointer on screen.
constexpr int kSnapBudgetPx = 8;
// How far along the bearing a steer destination is placed: out of reach in one
// hold, below GiveDistance2D's s16 limit.
constexpr int kSteerDistance = 12000;

// One cover zone's plane<->screen maps under one camera.
struct PolyFit
{
    std::vector<XZ> cover; // polygon, cover units (room / 10)
    Homography toScreen;   // room units -> screen
    Homography toFloor;
};

// Fit every cover zone at floor height floorY. Zones with fewer than four
// distinct vertices in front of the camera are skipped.
std::vector<PolyFit> fitFloor(const Camera& camera, const std::vector<std::vector<XZ>>& coverPolys, int floorY);

// The floor point under `pixel` (room units) inside a zone by the engine's
// isInPoly rule, or nothing.
std::optional<XZ> pickFloor(const std::vector<PolyFit>& fits, Point pixel);

// A far destination along the bearing from `here` toward `pixel` (for pixels
// with no reachable floor), or nothing when the hero's feet are off screen.
std::optional<XZ> steerPoint(const Camera& camera, const std::vector<PolyFit>& fits,
                             int floorY, XZ here, Point pixel);

} // namespace mouse
