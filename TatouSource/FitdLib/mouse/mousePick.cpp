///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Mouse gameplay: screen <-> floor picking. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "mousePick.h"

#include <cmath>
#include <utility>

namespace mouse
{

Camera frameCamera(int alpha, int beta, int gamma, int camX, int camY, int camZ,
                   int focal1, int focal2, int focal3, RoomOrigin room, const int16_t* cosTable)
{
    Camera c;
    c.alpha = alpha;
    c.beta = beta;
    c.gamma = gamma;
    c.posX = (camX - room.worldX) * 10;
    c.posY = (room.worldY - camY) * 10;
    c.posZ = (room.worldZ - camZ) * 10;
    c.focal1 = focal1;
    c.focal2 = focal2;
    c.focal3 = focal3;
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
}

std::optional<Vec2> projectPoint(const Camera& c, int wx, int wy, int wz)
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
    const double sx = (double)(int16_t)x;
    const double sy = (double)(int16_t)y;
    const double depth = (double)(int16_t)z + (double)c.focal1;
    if (depth <= 50.0)
        return std::nullopt;
    return Vec2{ sx * c.focal2 / depth + 160.0, sy * c.focal3 / depth + 100.0 };
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

} // namespace mouse
