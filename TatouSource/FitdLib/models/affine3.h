///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: 3x4 affine matrices. Engine-free: standard headers only.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cmath>

namespace models
{

// Row-major 3x4: p' = m[r][0] x + m[r][1] y + m[r][2] z + m[r][3].
template <typename T>
struct Affine
{
    T m[3][4];
};

using Affine3 = Affine<float>;   // what the GPU gets
using Affine3d = Affine<double>; // what the pose math accumulates in

struct Vec3
{
    float x, y, z;
};

template <typename T>
inline Affine<T> identityAffine()
{
    return Affine<T>{ { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 } } };
}

template <typename T>
inline Affine<T> translationAffine(T x, T y, T z)
{
    return Affine<T>{ { { 1, 0, 0, x }, { 0, 1, 0, y }, { 0, 0, 1, z } } };
}

// a ∘ b: b applies first.
template <typename T>
inline Affine<T> compose(const Affine<T>& a, const Affine<T>& b)
{
    Affine<T> r{};
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            T v = j == 3 ? a.m[i][3] : T(0);
            for (int k = 0; k < 3; ++k)
                v += a.m[i][k] * b.m[k][j];
            r.m[i][j] = v;
        }
    }
    return r;
}

template <typename T>
inline void applyAffine(const Affine<T>& a, T x, T y, T z, T out[3])
{
    for (int i = 0; i < 3; ++i)
        out[i] = a.m[i][0] * x + a.m[i][1] * y + a.m[i][2] * z + a.m[i][3];
}

inline Vec3 apply(const Affine3& a, Vec3 p)
{
    float o[3];
    applyAffine(a, p.x, p.y, p.z, o);
    return Vec3{ o[0], o[1], o[2] };
}

// false (and *out untouched) when the linear part is singular.
template <typename T>
inline bool invertAffine(const Affine<T>& a, Affine<T>* out)
{
    const T(&m)[3][4] = a.m;
    const T c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    const T c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
    const T c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    const T det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
    if (std::fabs(det) < T(1e-12))
        return false;
    const T inv = T(1) / det;
    Affine<T> r{};
    r.m[0][0] = c00 * inv;
    r.m[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv;
    r.m[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv;
    r.m[1][0] = c01 * inv;
    r.m[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv;
    r.m[1][2] = (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * inv;
    r.m[2][0] = c02 * inv;
    r.m[2][1] = (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * inv;
    r.m[2][2] = (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * inv;
    for (int i = 0; i < 3; ++i)
        r.m[i][3] = -(r.m[i][0] * m[0][3] + r.m[i][1] * m[1][3] + r.m[i][2] * m[2][3]);
    *out = r;
    return true;
}

template <typename To, typename From>
inline Affine<To> castAffine(const Affine<From>& a)
{
    Affine<To> r{};
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = (To)a.m[i][j];
    return r;
}

} // namespace models
