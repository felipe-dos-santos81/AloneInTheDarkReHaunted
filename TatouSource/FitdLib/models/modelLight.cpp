///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: lights in camera space. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "modelLight.h"

#include <cmath>

namespace models
{

Vec3 cameraDirection(const RenderCamera& cam, Vec3 worldDir)
{
    const Affine3 v = viewMatrix(cam);
    const float x = v.m[0][0] * worldDir.x + v.m[0][1] * worldDir.y + v.m[0][2] * worldDir.z;
    const float y = v.m[1][0] * worldDir.x + v.m[1][1] * worldDir.y + v.m[1][2] * worldDir.z;
    const float z = v.m[2][0] * worldDir.x + v.m[2][1] * worldDir.y + v.m[2][2] * worldDir.z;
    const float length = std::sqrt(x * x + y * y + z * z);
    if (length < 1e-6f)
        return Vec3{ 0.0f, 0.0f, 0.0f };
    return Vec3{ x / length, y / length, z / length };
}

Vec3 cameraPoint(const RenderCamera& cam, Vec3 worldPoint)
{
    return apply(viewMatrix(cam), worldPoint);
}

} // namespace models
