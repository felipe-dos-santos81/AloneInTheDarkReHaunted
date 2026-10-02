///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: bone matrices and screen box. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "skinnedBody.h"

#include <cmath>

namespace models
{

namespace
{
constexpr float kNearZ = 50.0f; // AnimNuage drops points with Z <= 50

void grow(Box3& b, const float p[3])
{
    for (int k = 0; k < 3; ++k)
    {
        if (b.empty || p[k] < b.lo[k])
            b.lo[k] = p[k];
        if (b.empty || p[k] > b.hi[k])
            b.hi[k] = p[k];
    }
    b.empty = false;
}

int clampInt(float v, int lo, int hi)
{
    const int i = (int)std::floor(v);
    return i < lo ? lo : (i > hi ? hi : i);
}
}

bool prepareSkin(const PoseBody& body, const HdmMesh& mesh, const int16_t* sinTable, SkinBind* out, std::string* why)
{
    const size_t groups = body.groups.size();
    if (mesh.groupCount != groups)
    {
        if (why)
            *why = "made for " + std::to_string(mesh.groupCount) + " groups, the body has " + std::to_string(groups);
        return false;
    }
    if (mesh.skeletonHash != skeletonHash(body))
    {
        if (why)
            *why = "made for another skeleton (hash differs)";
        return false;
    }
    SkinBind skin;
    skin.inverseBind.resize(groups);
    if (!restBind(body, nullptr, sinTable, skin.inverseBind.data()))
    {
        if (why)
            *why = "the body's rest pose has no inverse";
        return false;
    }
    skin.groupBoxes.resize(groups);
    for (const HdmVertex& v : mesh.vertices)
        for (int k = 0; k < 4; ++k)
            if (v.weights[k] > 0)
                grow(skin.groupBoxes[v.joints[k]], v.position);
    *out = std::move(skin);
    return true;
}

bool boneMatrices(const PoseBody& body, const GroupState* states, int alpha, int beta, int gamma, int x, int y,
                  int z, const RenderCamera& cam, const SkinBind& skin, Affine3* bones)
{
    const size_t groups = body.groups.size();
    std::vector<Affine3> world(groups), local(groups);
    if (!poseGroups(body, states, alpha, beta, gamma, cam.table, world.data()))
        return false;
    skinMatrices(world.data(), skin.inverseBind.data(), (int)groups, local.data());
    const Affine3d viewActor = compose(castAffine<double>(viewMatrix(cam)), translationAffine<double>(x, y, z));
    for (size_t g = 0; g < groups; ++g)
        bones[g] = castAffine<float>(compose(viewActor, castAffine<double>(local[g])));
    return true;
}

bool screenBox(const Affine3* bones, const SkinBind& skin, const ProjParams& p, int box[4])
{
    bool any = false;
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (size_t g = 0; g < skin.groupBoxes.size(); ++g)
    {
        const Box3& b = skin.groupBoxes[g];
        if (b.empty)
            continue;
        for (int c = 0; c < 8; ++c)
        {
            const Vec3 corner{ (c & 1) ? b.hi[0] : b.lo[0], (c & 2) ? b.hi[1] : b.lo[1], (c & 4) ? b.hi[2] : b.lo[2] };
            const Vec3 pc = apply(bones[g], corner);
            if (pc.z <= kNearZ)
                continue;
            const Vec4 clip = clipFromView(pc, p);
            const float sx = (clip.x / clip.w + 1.0f) * 160.0f;
            const float sy = (1.0f - clip.y / clip.w) * 100.0f;
            if (!any || sx < x0) x0 = sx;
            if (!any || sx > x1) x1 = sx;
            if (!any || sy < y0) y0 = sy;
            if (!any || sy > y1) y1 = sy;
            any = true;
        }
    }
    if (!any)
        return false;
    box[0] = clampInt(x0, 0, 319);
    box[1] = clampInt(y0, 0, 199);
    box[2] = clampInt(std::ceil(x1), 0, 319);
    box[3] = clampInt(std::ceil(y1), 0, 199);
    return true;
}

void columnMajor(const Affine3& a, float out[16])
{
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 3; ++r)
            out[c * 4 + r] = a.m[r][c];
        out[c * 4 + 3] = c == 3 ? 1.0f : 0.0f;
    }
}

} // namespace models
