///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: the engine's camera and projection in float. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "renderCamera.h"

namespace models
{

namespace
{
// transformPoint: (a, b) -> (a c - b s, a s + b c) on rows i and j, with the
// table read at the masked angle (a sine) and angle + 0x100 (a cosine).
Affine3d cameraAxis(int masked, int i, int j, const int16_t* t)
{
    const double s = t[masked] / 32768.0;
    const double c = t[(masked + 0x100) & 0x3FF] / 32768.0;
    Affine3d m = identityAffine<double>();
    m.m[i][i] = c;
    m.m[i][j] = -s;
    m.m[j][i] = s;
    m.m[j][j] = c;
    return m;
}
}

Affine3 viewMatrix(const RenderCamera& cam)
{
    Affine3d r = translationAffine<double>(-cam.posX, -cam.posY, -cam.posZ);
    const int ax = cam.alpha & 0x3FF;
    const int ay = cam.beta & 0x3FF;
    const int az = cam.gamma & 0x3FF;
    if (ay)
        r = compose(cameraAxis(ay, 0, 2, cam.table), r);
    if (ax)
        r = compose(cameraAxis(ax, 1, 2, cam.table), r);
    if (az)
        r = compose(cameraAxis(az, 0, 1, cam.table), r);
    return castAffine<float>(compose(translationAffine<double>(0, 0, cam.persp), r));
}

ProjParams projParams(const RenderCamera& cam, float shakeX, float shakeY)
{
    return ProjParams{ cam.fovX / 160.0f, cam.fovY / 100.0f, (cam.centerX + shakeX) / 160.0f - 1.0f,
                       1.0f - (cam.centerY + shakeY) / 100.0f };
}

Vec4 clipFromView(Vec3 pc, const ProjParams& p)
{
    const float z = pc.z;
    return Vec4{ pc.x * p.px + p.pz * z, -pc.y * p.py + p.pw * z, z * z / kDepthScale, z };
}

} // namespace models
