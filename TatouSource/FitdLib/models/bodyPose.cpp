///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: the pose of an AITD1 animated body. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "bodyPose.h"

#include <algorithm>

namespace models
{

namespace
{

bool fail(std::string* why, std::string text)
{
    if (why)
        *why = std::move(text);
    return false;
}

void put16(std::vector<uint8_t>& b, int v)
{
    b.push_back((uint8_t)(v & 0xFF));
    b.push_back((uint8_t)((v >> 8) & 0xFF));
}

} // namespace

bool validateSkeleton(const PoseBody& body, std::string* why)
{
    if (!(body.flags & kInfoAnim))
        return fail(why, "not animated (no INFO_ANIM)");
    if (body.flags & kInfoOptimise)
        return fail(why, "INFO_OPTIMISE (AITD2+) bodies are not supported");
    const int n = (int)body.groups.size();
    const int nv = (int)body.verts.size();
    if (n < 1 || n > kMaxPoseGroups)
        return fail(why, std::to_string(n) + " groups (must be 1.." + std::to_string(kMaxPoseGroups) + ")");
    std::vector<uint16_t> sorted = body.order;
    std::sort(sorted.begin(), sorted.end());
    for (int i = 0; i < n; ++i)
        if ((int)sorted.size() != n || sorted[i] != i)
            return fail(why, "group order is not a permutation of the groups");

    std::vector<int> owner(nv, -1);
    for (int gi = 0; gi < n; ++gi)
    {
        const PoseGroup& g = body.groups[gi];
        const std::string at = "group " + std::to_string(gi) + ": ";
        if (g.self != gi)
            return fail(why, at + "m_numGroup is " + std::to_string(g.self));
        if (gi == 0 && g.parent != -1)
            return fail(why, "root group has parent " + std::to_string(g.parent));
        if (gi && !(0 <= g.parent && g.parent < gi))
            return fail(why, at + "parent " + std::to_string(g.parent) + " is not an earlier group");
        if (g.count < 0 || g.start < 0 || g.start + g.count > nv)
            return fail(why, at + "vertex range " + std::to_string(g.start) + "+" + std::to_string(g.count)
                                 + " outside 0.." + std::to_string(nv));
        for (int v = g.start; v < g.start + g.count; ++v)
        {
            if (owner[v] != -1)
                return fail(why, "group vertex ranges do not cover every vertex exactly once");
            owner[v] = gi;
        }
    }
    if (std::count(owner.begin(), owner.end(), -1))
        return fail(why, "group vertex ranges do not cover every vertex exactly once");
    for (int gi = 1; gi < n; ++gi)
    {
        const PoseGroup& g = body.groups[gi];
        if (g.pivot < 0 || g.pivot >= nv)
            return fail(why, "group " + std::to_string(gi) + ": pivot " + std::to_string(g.pivot) + " out of range");
        if (owner[g.pivot] != g.parent)
            return fail(why, "group " + std::to_string(gi) + ": pivot vertex " + std::to_string(g.pivot)
                                 + " is not in parent group " + std::to_string(g.parent));
    }
    if (body.groups[0].pivot != 0 || body.verts[0] != std::array<int16_t, 3>{ 0, 0, 0 })
        return fail(why, "root pivot is not vertex 0 at the origin");
    // Leaves first: every group is processed before its parent, so its own
    // operation is the innermost one of its matrix.
    std::vector<int> position(n);
    for (int i = 0; i < n; ++i)
        position[body.order[i]] = i;
    for (int gi = 1; gi < n; ++gi)
        if (position[gi] > position[body.groups[gi].parent])
            return fail(why, "group " + std::to_string(body.groups[gi].parent)
                                 + " is processed before one of its descendants");
    return true;
}

uint64_t skeletonHash(const PoseBody& body)
{
    std::vector<uint8_t> blob;
    put16(blob, (int)body.verts.size());
    for (const auto& v : body.verts)
        for (int16_t c : v)
            put16(blob, c);
    put16(blob, (int)body.groups.size());
    for (const PoseGroup& g : body.groups)
    {
        put16(blob, g.start);
        put16(blob, g.count);
        put16(blob, g.pivot);
        blob.push_back((uint8_t)g.parent);
        blob.push_back((uint8_t)g.self);
    }
    for (uint16_t o : body.order)
        put16(blob, o);
    uint64_t h = 0xCBF29CE484222325ull;
    for (uint8_t byte : blob)
        h = (h ^ byte) * 0x100000001B3ull;
    return h;
}

namespace
{

// RotateList's matrix: Y, then X, then Z (R = Rz Rx Ry). An axis is skipped
// only when its raw angle is 0: 1024 reads table[0] = 4, a tiny rotation.
Affine3d rotation(int ax, int ay, int az, const int16_t* t)
{
    Affine3d r = identityAffine<double>();
    auto axis = [&](int angle, int i, int j) {
        // Rows i and j mix: (a, b) -> (a c - b s, a s + b c), as RotateList does.
        const double s = t[angle & 0x3FF] / 32768.0;
        const double c = t[(angle + 0x100) & 0x3FF] / 32768.0;
        Affine3d m = identityAffine<double>();
        m.m[i][i] = c;
        m.m[i][j] = -s;
        m.m[j][i] = s;
        m.m[j][j] = c;
        r = compose(m, r);
    };
    if (ay)
        axis(ay, 0, 2);
    if (ax)
        axis(ax, 1, 2);
    if (az)
        axis(az, 0, 1);
    return r;
}

// RotateGroupe: rotate the group at array position `pos`, then scan the next
// (groups - m_numGroup) entries, starting with itself, recursing into every
// entry whose parent is this group's number.
void rotateGroupe(const PoseBody& body, int pos, const Affine3d& r, Affine3d* local)
{
    const PoseGroup& g = body.groups[pos];
    local[pos] = compose(r, local[pos]);
    const int n = (int)body.groups.size();
    for (int j = pos; j < std::min(n, pos + n - g.self); ++j)
        if (body.groups[j].parent == g.self)
            rotateGroupe(body, j, r, local);
}

bool poseDouble(const PoseBody& body, const GroupState* states, int alpha, int beta, int gamma,
                const int16_t* sinTable, Affine3d* world)
{
    const int n = (int)body.groups.size();
    GroupState s[kMaxPoseGroups];
    std::copy(states, states + n, s);
    // The engine stores the actor's angles in group 0's s16 delta.
    s[0].dx = (int16_t)alpha;
    s[0].dy = (int16_t)beta;
    s[0].dz = (int16_t)gamma;
    if (s[0].type == 1 && (s[0].dx || s[0].dy || s[0].dz))
        return false;

    Affine3d local[kMaxPoseGroups];
    std::fill(local, local + n, identityAffine<double>());
    for (uint16_t h : body.order)
    {
        const GroupState& st = s[h];
        if (!st.dx && !st.dy && !st.dz)
            continue;
        switch (st.type)
        {
        case 0:
            rotateGroupe(body, h, rotation(st.dx, st.dy, st.dz, sinTable), local);
            break;
        case 1:
            local[h] = compose(translationAffine<double>(st.dx, st.dy, st.dz), local[h]);
            break;
        case 2:
        {
            Affine3d z = identityAffine<double>();
            z.m[0][0] = (st.dx + 256) / 256.0;
            z.m[1][1] = (st.dy + 256) / 256.0;
            z.m[2][2] = (st.dz + 256) / 256.0;
            local[h] = compose(z, local[h]);
            break;
        }
        default:
            break; // the engine's switch ignores other types
        }
    }

    // Pivot pass, in array order: a group hangs from its pivot vertex, which
    // belongs to the (already placed) parent.
    for (int g = 0; g < n; ++g)
    {
        double p[3] = { 0.0, 0.0, 0.0 };
        if (g)
        {
            const auto& v = body.verts[body.groups[g].pivot];
            applyAffine(world[body.groups[g].parent], (double)v[0], (double)v[1], (double)v[2], p);
        }
        world[g] = compose(translationAffine(p[0], p[1], p[2]), local[g]);
    }
    return true;
}

} // namespace

bool poseGroups(const PoseBody& body, const GroupState* states, int alpha, int beta, int gamma,
                const int16_t* sinTable, Affine3* worldFromLocal)
{
    Affine3d world[kMaxPoseGroups];
    if (!poseDouble(body, states, alpha, beta, gamma, sinTable, world))
        return false;
    for (size_t g = 0; g < body.groups.size(); ++g)
        worldFromLocal[g] = castAffine<float>(world[g]);
    return true;
}

} // namespace models
