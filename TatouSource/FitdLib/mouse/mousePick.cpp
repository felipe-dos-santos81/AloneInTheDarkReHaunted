///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: screen <-> floor picking. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mousePick.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace mouse
{

Camera frameCamera(const CameraData& cam, RoomOrigin room, const int16_t* cosTable)
{
    Camera c;
    c.alpha = cam.alpha;
    c.beta = cam.beta;
    c.gamma = cam.gamma;
    c.posX = (cam.x - room.worldX) * 10;
    c.posY = (room.worldY - cam.y) * 10;
    c.posZ = (room.worldZ - cam.z) * 10;
    c.focal1 = cam.focal1;
    c.focal2 = cam.focal2;
    c.focal3 = cam.focal3;
    c.cosTable = cosTable;
    return c;
}

namespace
{
// One transformPoint rotation: a' = (a*sn - b*cs)/0x10000*2, b' = (a*cs + b*sn)/0x10000*2,
// with C truncating division. 64-bit products avoid the engine's int overflow UB.
inline void rotatePair(int a, int b, int64_t cs, int64_t sn, int* outA, int* outB)
{
    *outA = (int)((((int64_t)a * sn) - ((int64_t)b * cs)) / 0x10000) * 2;
    *outB = (int)((((int64_t)a * cs) + ((int64_t)b * sn)) / 0x10000) * 2;
}

// The renderer's near plane: it divides only depths above this.
constexpr double kNearDepth = 50.0;

// A room-frame point in the renderer's camera space: x, y and the divide's depth.
struct CameraPoint
{
    double x = 0.0;
    double y = 0.0;
    double depth = 0.0;
};

// Camera space before the near-plane cull, or nothing under the height clamp.
std::optional<CameraPoint> toCameraSpaceUnculled(const Camera& c, int wx, int wy, int wz)
{
    // renderer.cpp: X += x - translateX, Y = y, Z += z - translateZ; height clamp; Y -= translateY
    int X = wx - c.posX;
    int Y = wy;
    int Z = wz - c.posZ;
    if (Y > 10000)
        return std::nullopt;
    Y -= c.posY;

    // transformPoint: beta (Y axis), then alpha (X axis), then gamma (Z axis)
    const int16_t* t = c.cosTable;
    const int ay = c.beta & 0x3FF;
    const int ax = c.alpha & 0x3FF;
    const int az = c.gamma & 0x3FF;
    int x = X;
    int y = Y;
    int z = Z;
    if (ay)
        rotatePair(X, Z, t[ay], t[(ay + 0x100) & 0x3FF], &x, &z);
    if (ax)
        rotatePair(Y, z, t[ax], t[(ax + 0x100) & 0x3FF], &y, &z);
    if (az)
    {
        int nx = 0;
        int ny = 0;
        rotatePair(x, y, t[az], t[(az + 0x100) & 0x3FF], &nx, &ny);
        x = nx;
        y = ny;
    }

    // The renderer stores camera-space coordinates as s16, then divides in float.
    return CameraPoint{ (double)(int16_t)x, (double)(int16_t)y, (double)(int16_t)z + (double)c.focal1 };
}

// Camera space, or nothing when culled (height clamp, depth <= 50).
std::optional<CameraPoint> toCameraSpace(const Camera& c, int wx, int wy, int wz)
{
    auto p = toCameraSpaceUnculled(c, wx, wy, wz);
    if (!p || p->depth <= kNearDepth)
        return std::nullopt;
    return p;
}

Vec2 divide(const Camera& c, const CameraPoint& p)
{
    return Vec2{ p.x * c.focal2 / p.depth + 160.0, p.y * c.focal3 / p.depth + 100.0 };
}

double cross(const Vec2& o, const Vec2& a, const Vec2& b)
{
    return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
}

// Monotone-chain convex hull, counter-clockwise; fewer than three points when
// the input is collinear.
std::vector<Vec2> convexHull(std::vector<Vec2> points)
{
    std::sort(points.begin(), points.end(),
              [](const Vec2& a, const Vec2& b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    points.erase(std::unique(points.begin(), points.end(),
                             [](const Vec2& a, const Vec2& b) { return a.x == b.x && a.y == b.y; }),
                 points.end());
    if (points.size() < 3)
        return points;
    std::vector<Vec2> hull;
    for (int pass = 0; pass < 2; ++pass)
    {
        const size_t chainStart = hull.size();
        for (const Vec2& c : points)
        {
            while (hull.size() >= chainStart + 2 && cross(hull[hull.size() - 2], hull.back(), c) <= 0.0)
                hull.pop_back();
            hull.push_back(c);
        }
        hull.pop_back(); // the next chain starts with it
        std::reverse(points.begin(), points.end());
    }
    return hull;
}

bool hullContains(const std::vector<Vec2>& hull, const Vec2& p)
{
    if (hull.size() < 3)
        return false;
    for (size_t i = 0; i < hull.size(); ++i)
        if (cross(hull[i], hull[(i + 1) % hull.size()], p) < 0.0)
            return false;
    return true;
}

double segmentDistance(const Vec2& a, const Vec2& b, const Vec2& p)
{
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double length2 = dx * dx + dy * dy;
    const double t = length2 > 0.0 ? std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / length2, 0.0, 1.0) : 0.0;
    return std::hypot(a.x + t * dx - p.x, a.y + t * dy - p.y);
}
}

std::optional<Vec2> projectPoint(const Camera& c, int wx, int wy, int wz)
{
    auto p = toCameraSpace(c, wx, wy, wz);
    if (!p)
        return std::nullopt;
    return divide(c, *p);
}

bool nearerThanBox(const Camera& camera, const Box& box, int x, int y, int z)
{
    auto point = toCameraSpace(camera, x, y, z);
    if (!point)
        return false;
    for (int cx : { box.x1, box.x2 })
        for (int cy : { box.y1, box.y2 })
            for (int cz : { box.z1, box.z2 })
            {
                auto corner = toCameraSpace(camera, cx, cy, cz);
                if (corner && corner->depth <= point->depth)
                    return false;
            }
    return true;
}

namespace
{
struct Norm
{
    double cx = 0.0;
    double cy = 0.0;
    double s = 0.0;
};

// Hartley normalisation: centroid to the origin, mean distance sqrt(2).
Norm normFor(const std::array<Vec2, 4>& p)
{
    Norm n;
    for (const Vec2& v : p)
    {
        n.cx += v.x / 4.0;
        n.cy += v.y / 4.0;
    }
    double mean = 0.0;
    for (const Vec2& v : p)
        mean += std::hypot(v.x - n.cx, v.y - n.cy) / 4.0;
    n.s = mean > 0.0 ? std::sqrt(2.0) / mean : 0.0;
    return n;
}

std::array<double, 9> multiply(const std::array<double, 9>& a, const std::array<double, 9>& b)
{
    std::array<double, 9> r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            for (int k = 0; k < 3; ++k)
                r[i * 3 + j] += a[i * 3 + k] * b[k * 3 + j];
    return r;
}
}

std::optional<Homography> fitHomography(const std::array<Vec2, 4>& src, const std::array<Vec2, 4>& dst)
{
    const Norm ns = normFor(src);
    const Norm nd = normFor(dst);
    if (ns.s == 0.0 || nd.s == 0.0)
        return std::nullopt;

    // 8x8 system (column 8 is the right-hand side) for h with h33 = 1.
    double a[8][9];
    for (int i = 0; i < 4; ++i)
    {
        const double x = (src[i].x - ns.cx) * ns.s;
        const double y = (src[i].y - ns.cy) * ns.s;
        const double u = (dst[i].x - nd.cx) * nd.s;
        const double v = (dst[i].y - nd.cy) * nd.s;
        const double r0[9] = { x, y, 1.0, 0.0, 0.0, 0.0, -u * x, -u * y, u };
        const double r1[9] = { 0.0, 0.0, 0.0, x, y, 1.0, -v * x, -v * y, v };
        for (int k = 0; k < 9; ++k)
        {
            a[2 * i][k] = r0[k];
            a[2 * i + 1][k] = r1[k];
        }
    }
    for (int col = 0; col < 8; ++col)
    {
        int pivot = col;
        for (int r = col + 1; r < 8; ++r)
            if (std::fabs(a[r][col]) > std::fabs(a[pivot][col]))
                pivot = r;
        if (std::fabs(a[pivot][col]) < 1e-9)
            return std::nullopt; // degenerate (collinear) correspondences
        if (pivot != col)
            for (int k = 0; k < 9; ++k)
                std::swap(a[col][k], a[pivot][k]);
        for (int r = 0; r < 8; ++r)
        {
            if (r == col)
                continue;
            const double f = a[r][col] / a[col][col];
            if (f == 0.0)
                continue;
            for (int k = col; k < 9; ++k)
                a[r][k] -= f * a[col][k];
        }
    }
    std::array<double, 9> hn{};
    for (int i = 0; i < 8; ++i)
        hn[i] = a[i][8] / a[i][i];
    hn[8] = 1.0;

    // H = Td^-1 * Hn * Ts
    const std::array<double, 9> ts = { ns.s, 0.0, -ns.s * ns.cx, 0.0, ns.s, -ns.s * ns.cy, 0.0, 0.0, 1.0 };
    const std::array<double, 9> tdInv = { 1.0 / nd.s, 0.0, nd.cx, 0.0, 1.0 / nd.s, nd.cy, 0.0, 0.0, 1.0 };
    std::array<double, 9> m = multiply(tdInv, multiply(hn, ts));
    if (std::fabs(m[8]) < 1e-15)
        return std::nullopt;
    Homography h;
    for (int i = 0; i < 9; ++i)
        h.m[i] = m[i] / m[8];
    return h;
}

std::optional<Homography> invert(const Homography& h)
{
    const auto& m = h.m;
    const double det = m[0] * (m[4] * m[8] - m[5] * m[7])
                     - m[1] * (m[3] * m[8] - m[5] * m[6])
                     + m[2] * (m[3] * m[7] - m[4] * m[6]);
    if (std::fabs(det) < 1e-18)
        return std::nullopt;
    Homography r;
    r.m[0] = (m[4] * m[8] - m[5] * m[7]) / det;
    r.m[1] = (m[2] * m[7] - m[1] * m[8]) / det;
    r.m[2] = (m[1] * m[5] - m[2] * m[4]) / det;
    r.m[3] = (m[5] * m[6] - m[3] * m[8]) / det;
    r.m[4] = (m[0] * m[8] - m[2] * m[6]) / det;
    r.m[5] = (m[2] * m[3] - m[0] * m[5]) / det;
    r.m[6] = (m[3] * m[7] - m[4] * m[6]) / det;
    r.m[7] = (m[1] * m[6] - m[0] * m[7]) / det;
    r.m[8] = (m[0] * m[4] - m[1] * m[3]) / det;
    return r;
}

std::optional<Vec2> apply(const Homography& h, double x, double y)
{
    const double w = h.m[6] * x + h.m[7] * y + h.m[8];
    if (std::fabs(w) < 1e-12)
        return std::nullopt;
    return Vec2{ (h.m[0] * x + h.m[1] * y + h.m[2]) / w, (h.m[3] * x + h.m[4] * y + h.m[5]) / w };
}

XZ reframe(XZ p, RoomOrigin from, RoomOrigin to)
{
    return XZ{ p.x - 10 * (to.worldX - from.worldX), p.z + 10 * (to.worldZ - from.worldZ) };
}

int reframeY(int y, RoomOrigin from, RoomOrigin to)
{
    return y + 10 * (to.worldY - from.worldY);
}

std::vector<PolyFit> fitFloor(const Camera& camera, const std::vector<std::vector<XZ>>& coverPolys, int floorY)
{
    std::vector<PolyFit> fits;
    for (const std::vector<XZ>& poly : coverPolys)
    {
        std::vector<std::pair<XZ, Vec2>> usable;
        for (XZ c : poly)
        {
            const XZ w{ c.x * kCoverScale, c.z * kCoverScale };
            if (auto s = projectPoint(camera, w.x, floorY, w.z))
                usable.push_back({ w, *s });
        }
        if (usable.size() < 4)
            continue;

        // Farthest-point sampling in world space: any four coplanar points
        // define the plane's homography; spreading them conditions the fit.
        std::vector<size_t> chosen{ 0 };
        while (chosen.size() < 4)
        {
            size_t best = std::numeric_limits<size_t>::max();
            double bestScore = 0.0;
            for (size_t i = 0; i < usable.size(); ++i)
            {
                if (std::find(chosen.begin(), chosen.end(), i) != chosen.end())
                    continue;
                double score = std::numeric_limits<double>::max();
                for (size_t c : chosen)
                {
                    const double dx = usable[i].first.x - usable[c].first.x;
                    const double dz = usable[i].first.z - usable[c].first.z;
                    score = std::min(score, dx * dx + dz * dz);
                }
                if (score > bestScore)
                {
                    best = i;
                    bestScore = score;
                }
            }
            if (best == std::numeric_limits<size_t>::max())
                break; // fewer than four distinct vertices
            chosen.push_back(best);
        }
        if (chosen.size() < 4)
            continue;

        std::array<Vec2, 4> src;
        std::array<Vec2, 4> dst;
        for (int k = 0; k < 4; ++k)
        {
            src[k] = Vec2{ (double)usable[chosen[k]].first.x, (double)usable[chosen[k]].first.z };
            dst[k] = usable[chosen[k]].second;
        }
        auto toScreen = fitHomography(src, dst);
        if (!toScreen)
            continue;
        auto toFloor = invert(*toScreen);
        if (!toFloor)
            continue;
        fits.push_back(PolyFit{ poly, *toScreen, *toFloor });
    }
    return fits;
}

std::optional<XZ> pickFloor(const std::vector<PolyFit>& fits, Point pixel, const std::function<bool(XZ)>& alsoFloor)
{
    for (const PolyFit& fit : fits)
    {
        auto recovered = apply(fit.toFloor, pixel.x, pixel.y);
        if (!recovered)
            continue;
        const int wx = (int)std::lround(recovered->x);
        const int wz = (int)std::lround(recovered->y);
        auto forward = apply(fit.toScreen, wx, wz);
        if (!forward)
            continue;
        if (std::fabs(forward->x - pixel.x) > kReprojectPx || std::fabs(forward->y - pixel.y) > kReprojectPx)
            continue; // the fit does not explain this pixel
        if (insideCoverZone(wx, wz, fit.cover) || (alsoFloor && alsoFloor(XZ{ wx, wz })))
            return XZ{ wx, wz };
    }
    return std::nullopt;
}

bool boxSilhouetteContains(const Camera& camera, const Box& box, Point pixel)
{
    std::vector<Vec2> corners;
    for (int x : { box.x1, box.x2 })
        for (int y : { box.y1, box.y2 })
            for (int z : { box.z1, box.z2 })
            {
                auto s = projectPoint(camera, x, y, z);
                if (!s)
                    return false;
                corners.push_back(*s);
            }
    return hullContains(convexHull(std::move(corners)), Vec2{ (double)pixel.x, (double)pixel.y });
}

Box posedBox(const Box& body, int alpha, int beta, int gamma, int x, int y, int z, const int16_t* cosTable)
{
    if (!alpha && !beta && !gamma)
        return Box{ body.x1 + x, body.x2 + x, body.y1 + y, body.y2 + y, body.z1 + z, body.z2 + z };
    auto turn = [cosTable](int angle, double* a, double* b) {
        const double cs = cosTable[angle & 0x3FF];
        const double sn = cosTable[(angle + 0x100) & 0x3FF];
        const double oldA = *a;
        *a = (sn * oldA - cs * *b) / 65536.0 * 2.0;
        *b = (cs * oldA + sn * *b) / 65536.0 * 2.0;
    };
    double lo[3] = { 1e9, 1e9, 1e9 };
    double hi[3] = { -1e9, -1e9, -1e9 };
    for (int k = 0; k < 8; ++k)
    {
        double v[3] = { (double)((k & 1) ? body.x2 : body.x1), (double)((k & 2) ? body.y2 : body.y1),
                        (double)((k & 4) ? body.z2 : body.z1) };
        turn(beta, &v[0], &v[2]);  // Y rotation
        turn(gamma, &v[0], &v[1]); // Z rotation
        turn(alpha, &v[1], &v[2]); // X rotation
        for (int axis = 0; axis < 3; ++axis)
        {
            lo[axis] = std::min(lo[axis], v[axis]);
            hi[axis] = std::max(hi[axis], v[axis]);
        }
    }
    return Box{ (int)std::floor(lo[0]) + x, (int)std::ceil(hi[0]) + x, (int)std::floor(lo[1]) + y,
                (int)std::ceil(hi[1]) + y, (int)std::floor(lo[2]) + z, (int)std::ceil(hi[2]) + z };
}

std::vector<Vec2> clippedBoxOutline(const Camera& camera, const Box& box)
{
    std::optional<CameraPoint> corners[8];
    for (int k = 0; k < 8; ++k)
    {
        corners[k] = toCameraSpaceUnculled(camera, (k & 1) ? box.x2 : box.x1, (k & 2) ? box.y2 : box.y1,
                                           (k & 4) ? box.z2 : box.z1);
        if (!corners[k])
            return {}; // under the height clamp: never drawn
    }
    std::vector<Vec2> points;
    for (int k = 0; k < 8; ++k)
    {
        const CameraPoint& a = *corners[k];
        if (a.depth > kNearDepth)
            points.push_back(divide(camera, a));
        for (int axis : { 1, 2, 4 }) // each edge once, from its lower corner
        {
            if (k & axis)
                continue;
            const CameraPoint& b = *corners[k | axis];
            if ((a.depth > kNearDepth) == (b.depth > kNearDepth))
                continue;
            // Camera space is affine in the room frame: cut the edge just in
            // front of the plane (the divide is undefined on it).
            const double cut = kNearDepth + 1.0;
            const double t = (cut - a.depth) / (b.depth - a.depth);
            points.push_back(divide(camera, CameraPoint{ a.x + t * (b.x - a.x), a.y + t * (b.y - a.y), cut }));
        }
    }
    return convexHull(std::move(points));
}

bool outlineContains(const std::vector<Vec2>& outline, Point pixel, double slack)
{
    const Vec2 p{ (double)pixel.x, (double)pixel.y };
    if (hullContains(outline, p))
        return true;
    if (outline.empty() || slack <= 0.0)
        return false;
    // A point, a segment (an edge-on box) or a closed hull.
    const size_t edges = outline.size() < 3 ? outline.size() - 1 : outline.size();
    double nearest = std::hypot(outline[0].x - p.x, outline[0].y - p.y);
    for (size_t i = 0; i < edges; ++i)
        nearest = std::min(nearest, segmentDistance(outline[i], outline[(i + 1) % outline.size()], p));
    return nearest <= slack;
}

std::optional<XZ> steerPoint(const Camera& camera, const std::vector<PolyFit>& fits,
                             int floorY, XZ here, Point pixel)
{
    if (fits.empty())
        return std::nullopt;
    auto feet = projectPoint(camera, here.x, floorY, here.z);
    if (!feet)
        return std::nullopt;
    // Every polygon of a room shares one plane, so any fit's inverse answers.
    const Homography& toFloor = fits.front().toFloor;
    for (int sample = 0; sample < 12; ++sample)
    {
        const double weight = std::pow(0.5, sample); // 1 = the pixel, 0 = the feet
        const double px = feet->x + (pixel.x - feet->x) * weight;
        const double py = feet->y + (pixel.y - feet->y) * weight;
        auto recovered = apply(toFloor, px, py);
        if (!recovered)
            continue;
        if (!projectPoint(camera, (int)std::lround(recovered->x), floorY, (int)std::lround(recovered->y)))
            continue; // behind the camera: this pixel is above the horizon
        const double dx = recovered->x - here.x;
        const double dz = recovered->y - here.z;
        const double length = std::hypot(dx, dz);
        if (length < 1.0)
            continue; // the pointer is on the hero
        return XZ{ (int)std::lround(here.x + dx * kSteerDistance / length),
                   (int)std::lround(here.z + dz * kSteerDistance / length) };
    }
    return std::nullopt;
}


} // namespace mouse
