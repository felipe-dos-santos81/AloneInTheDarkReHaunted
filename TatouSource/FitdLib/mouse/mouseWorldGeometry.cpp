///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: the engine's rooms, cameras and hero as the mouse modules
// see them, with the walk-grid and floor-fit caches.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "menuMouse.h"
#include "mouseWorldInternal.h"

#include <algorithm>
#include <map>
#include <tuple>

namespace mouseworld
{
namespace
{
// Cover polygons of every floor camera viewing `room`, in cover units (room/10).
std::vector<std::vector<mouse::XZ>> coverPolys(int room)
{
    std::vector<std::vector<mouse::XZ>> polys;
    for (const cameraDataStruct& cam : g_currentFloorCameraData)
        for (const cameraViewedRoomStruct& viewed : cam.viewedRoomTable)
        {
            if (viewed.viewedRoomIdx != room)
                continue;
            for (const cameraZoneEntryStruct& zone : viewed.coverZones)
            {
                std::vector<mouse::XZ> poly;
                for (int k = 0; k < zone.numPoints; ++k)
                    poly.push_back(mouse::XZ{ zone.pointTable[k].x, zone.pointTable[k].y });
                if (poly.size() >= 3)
                    polys.push_back(std::move(poly));
            }
        }
    return polys;
}

std::vector<mouse::Box> hardCols(int room)
{
    std::vector<mouse::Box> boxes;
    for (const hardColStruct& col : roomDataTable[room].hardColTable)
        boxes.push_back(mouse::Box{ col.zv.ZVX1, col.zv.ZVX2, col.zv.ZVY1, col.zv.ZVY2, col.zv.ZVZ1, col.zv.ZVZ2 });
    return boxes;
}

// One walk grid: a room on a floor for an agent's footprint and Y band. The
// band is bucketed by 100: the hero's exact y1/y2 crawl every hover frame on
// stairs and falls, and a rebuild costs ~4.5 ms, while the walkable band rarely
// changes cell. A bucket's grid is built for the first agent that lands in it.
struct GridKey
{
    int floor;
    int room;
    int half;
    int bandLow;
    int bandHigh;
};

bool operator<(const GridKey& a, const GridKey& b)
{
    return std::tie(a.floor, a.room, a.half, a.bandLow, a.bandHigh) <
           std::tie(b.floor, b.room, b.half, b.bandLow, b.bandHigh);
}

std::map<GridKey, std::optional<mouse::Grid>> s_grids;

// The reachable cells of one grid from one start cell (targetFor asks every
// hover frame; the answer changes only when the hero changes cell).
struct ReachCache
{
    const mouse::Grid* grid = nullptr;
    int i = -1;
    int j = -1;
    std::optional<mouse::Reach> reach;
};

ReachCache s_reach;

void clearGrids()
{
    s_grids.clear();
    s_reach = ReachCache{}; // it points into s_grids
}

// One room's floor fits at one height, under the on-screen camera.
struct FitKey
{
    int room;
    int floorY;
};

bool operator<(const FitKey& a, const FitKey& b)
{
    return std::tie(a.room, a.floorY) < std::tie(b.room, b.floorY);
}

std::map<FitKey, std::vector<mouse::PolyFit>> s_fits;

int s_fitsCamera = -1;
}

bool roomValid(int room)
{
    return room >= 0 && room < (int)roomDataTable.size();
}

mouse::RoomOrigin originOf(int room)
{
    const roomDataStruct& r = roomDataTable[room];
    return mouse::RoomOrigin{ r.worldX, r.worldY, r.worldZ };
}

mouse::Agent agentOf(const tObject& actor)
{
    const int halfX = (actor.zv.ZVX2 - actor.zv.ZVX1) / 2;
    const int halfZ = (actor.zv.ZVZ2 - actor.zv.ZVZ1) / 2;
    return mouse::Agent{ std::max(halfX, halfZ), actor.zv.ZVY1, actor.zv.ZVY2 };
}

// The hero's footprint with its Y band re-framed into `room`.
mouse::Agent agentIn(int room)
{
    mouse::Agent agent = agentOf(hero());
    if (room != hero().room)
    {
        agent.y1 = mouse::reframeY(agent.y1, originOf(hero().room), originOf(room));
        agent.y2 = mouse::reframeY(agent.y2, originOf(hero().room), originOf(room));
    }
    return agent;
}

const mouse::Reach* reachFor(const mouse::Grid& grid, mouse::XZ from)
{
    int i = -1;
    int j = -1;
    grid.cellOf(from.x, from.z, &i, &j);
    if (s_reach.grid != &grid || s_reach.i != i || s_reach.j != j)
        s_reach = ReachCache{ &grid, i, j, mouse::reachFrom(grid, from) };
    return s_reach.reach ? &*s_reach.reach : nullptr;
}

const mouse::Grid* gridFor(int room, const mouse::Agent& agent)
{
    if (!roomValid(room))
        return nullptr;
    const GridKey key{ (int)g_currentFloor, room, agent.half, mouse::floorDiv(agent.y1, 100),
                       mouse::floorDiv(agent.y2, 100) };
    auto it = s_grids.find(key);
    if (it == s_grids.end())
    {
        if (s_grids.size() > 64)
            clearGrids(); // the hero's Y band changes on stairs: keep this bounded
        it = s_grids.emplace(key, mouse::buildGrid(coverPolys(room), hardCols(room), agent)).first;
    }
    return it->second ? &*it->second : nullptr;
}

// Floor-camera index of the camera on screen, or -1.
int currentFloorCamera()
{
    if (NumCamera < 0 || !roomValid(currentRoom))
        return -1;
    const std::vector<u16>& slots = roomDataTable[currentRoom].cameraIdxTable;
    if (NumCamera >= (int)slots.size())
        return -1;
    const int idx = slots[NumCamera];
    return idx < (int)g_currentFloorCameraData.size() ? idx : -1;
}

// The on-screen camera framed in `room`'s coordinate space.
bool cameraForRoom(int room, mouse::Camera* out)
{
    const int idx = currentFloorCamera();
    if (idx < 0 || !roomValid(room))
        return false;
    const cameraDataStruct& c = g_currentFloorCameraData[idx];
    *out = mouse::frameCamera(mouse::CameraData{ c.alpha, c.beta, c.gamma, c.x, c.y, c.z, c.focal1, c.focal2, c.focal3 },
                              originOf(room), cosTable);
    return true;
}

// Plane fits of `room`'s floor at height floorY under the on-screen camera.
const std::vector<mouse::PolyFit>* fitsFor(int room, int floorY)
{
    const int cam = currentFloorCamera();
    if (cam != s_fitsCamera)
    {
        s_fits.clear();
        s_fitsCamera = cam;
    }
    if (cam < 0 || !roomValid(room))
        return nullptr;
    const FitKey key{ room, floorY };
    auto it = s_fits.find(key);
    if (it == s_fits.end())
    {
        mouse::Camera camera;
        if (!cameraForRoom(room, &camera))
            return nullptr;
        if (s_fits.size() > 64)
            s_fits.clear();
        it = s_fits.emplace(key, mouse::fitFloor(camera, coverPolys(room), floorY)).first;
    }
    return &it->second;
}

// Where a room-frame floor point shows on the 320x200 screen, when it does.
std::optional<mouse::Vec2> visibleAt(const mouse::Camera& camera, mouse::XZ p, int floorY)
{
    auto s = mouse::projectPoint(camera, p.x, floorY, p.z);
    if (!s || s->x < 0.0 || s->x >= mouse::kLogicalW || s->y < 0.0 || s->y >= mouse::kLogicalH)
        return std::nullopt;
    return s;
}

// A room-frame floor point on the logical screen, or nothing.
std::optional<mouse::Vec2> screenOf(int room, mouse::XZ p)
{
    if (!heroAvailable() || !roomValid(room))
        return std::nullopt;
    mouse::Camera camera;
    if (!cameraForRoom(room, &camera))
        return std::nullopt;
    const int floorY = mouse::reframeY(hero().roomY, originOf(hero().room), originOf(room));
    return mouse::projectPoint(camera, p.x, floorY, p.z);
}

// The doorway midpoint linking `from` to `to`, in from's frame (track.cpp follow mode).
mouse::XZ linkMidpoint(int from, int to)
{
    int x = 0;
    int y = 0;
    int z = 0;
    getRoomLinkCenter((unsigned int)from, (unsigned int)to, &x, &y, &z);
    return mouse::XZ{ x, z };
}

bool heroAvailable()
{
    return currentCameraTargetActor >= 0 && currentCameraTargetActor < NUM_MAX_OBJECT &&
           ListObjets[currentCameraTargetActor].indexInWorld >= 0 &&
           roomValid(ListObjets[currentCameraTargetActor].room);
}

tObject& hero()
{
    return ListObjets[currentCameraTargetActor];
}

mouse::HeroPose heroPose()
{
    const tObject& h = hero();
    return mouse::HeroPose{ h.room, mouse::XZ{ h.roomX + h.stepX, h.roomZ + h.stepZ }, h.beta };
}

void clearGeometryCaches()
{
    clearGrids();
    s_fits.clear();
    s_fitsCamera = -1;
}

} // namespace mouseworld
