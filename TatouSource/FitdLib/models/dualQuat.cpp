///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// See dualQuat.h.
///////////////////////////////////////////////////////////////////////////////
#include "dualQuat.h"

#include <cmath>

namespace models
{

namespace
{

// q1 q2 for (x, y, z, w) quaternions.
void multiply(const float a[4], const float b[4], float out[4])
{
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

float columnLength(const Affine3& m, int c)
{
    return std::sqrt(m.m[0][c] * m.m[0][c] + m.m[1][c] * m.m[1][c] + m.m[2][c] * m.m[2][c]);
}

} // namespace

DualQuat dualQuat(const Affine3& m)
{
    // Shepperd's method: the largest of w, x, y, z first, for precision.
    const float (&r)[3][4] = m.m;
    const float trace = r[0][0] + r[1][1] + r[2][2];
    float q[4];
    if (trace > 0.0f)
    {
        const float s = 2.0f * std::sqrt(1.0f + trace);
        q[3] = 0.25f * s;
        q[0] = (r[2][1] - r[1][2]) / s;
        q[1] = (r[0][2] - r[2][0]) / s;
        q[2] = (r[1][0] - r[0][1]) / s;
    }
    else if (r[0][0] > r[1][1] && r[0][0] > r[2][2])
    {
        const float s = 2.0f * std::sqrt(1.0f + r[0][0] - r[1][1] - r[2][2]);
        q[3] = (r[2][1] - r[1][2]) / s;
        q[0] = 0.25f * s;
        q[1] = (r[0][1] + r[1][0]) / s;
        q[2] = (r[0][2] + r[2][0]) / s;
    }
    else if (r[1][1] > r[2][2])
    {
        const float s = 2.0f * std::sqrt(1.0f + r[1][1] - r[0][0] - r[2][2]);
        q[3] = (r[0][2] - r[2][0]) / s;
        q[0] = (r[0][1] + r[1][0]) / s;
        q[1] = 0.25f * s;
        q[2] = (r[1][2] + r[2][1]) / s;
    }
    else
    {
        const float s = 2.0f * std::sqrt(1.0f + r[2][2] - r[0][0] - r[1][1]);
        q[3] = (r[1][0] - r[0][1]) / s;
        q[0] = (r[0][2] + r[2][0]) / s;
        q[1] = (r[1][2] + r[2][1]) / s;
        q[2] = 0.25f * s;
    }
    const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    DualQuat out{};
    for (int i = 0; i < 4; ++i)
        out.real[i] = q[i] / len;
    const float t[4] = { 0.5f * r[0][3], 0.5f * r[1][3], 0.5f * r[2][3], 0.0f };
    multiply(t, out.real, out.dual);
    return out;
}

bool zooms(const Affine3& m)
{
    for (int c = 0; c < 3; ++c)
        if (std::fabs(columnLength(m, c) - 1.0f) > kZoomTolerance)
            return true;
    return false;
}

Vec3 blendApply(const DualQuat* q, const float* w, int count, Vec3 p)
{
    float r[4] = { 0, 0, 0, 0 }, d[4] = { 0, 0, 0, 0 };
    for (int k = 0; k < count; ++k)
    {
        float dot = 0.0f;
        for (int i = 0; i < 4; ++i)
            dot += q[k].real[i] * q[0].real[i];
        const float s = dot < 0.0f ? -w[k] : w[k];
        for (int i = 0; i < 4; ++i)
        {
            r[i] += s * q[k].real[i];
            d[i] += s * q[k].dual[i];
        }
    }
    const float len = std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2] + r[3] * r[3]);
    for (int i = 0; i < 4; ++i)
    {
        r[i] /= len;
        d[i] /= len;
    }
    // rotate: p + 2 v x (v x p + w p); translate: 2 (w dv - dw v + v x dv)
    const float vx = r[0], vy = r[1], vz = r[2], vw = r[3];
    const float cx = vy * p.z - vz * p.y + vw * p.x, cy = vz * p.x - vx * p.z + vw * p.y, cz = vx * p.y - vy * p.x + vw * p.z;
    const Vec3 rotated{ p.x + 2 * (vy * cz - vz * cy), p.y + 2 * (vz * cx - vx * cz), p.z + 2 * (vx * cy - vy * cx) };
    const float tx = 2 * (vw * d[0] - d[3] * vx + vy * d[2] - vz * d[1]);
    const float ty = 2 * (vw * d[1] - d[3] * vy + vz * d[0] - vx * d[2]);
    const float tz = 2 * (vw * d[2] - d[3] * vz + vx * d[1] - vy * d[0]);
    return { rotated.x + tx, rotated.y + ty, rotated.z + tz };
}

} // namespace models
