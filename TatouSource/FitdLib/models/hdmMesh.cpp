///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: reading and validating body_<KEY>.hdm. Engine-free.
///////////////////////////////////////////////////////////////////////////////

#include "hdmMesh.h"

#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace models
{

namespace
{
constexpr size_t kHeaderSize = 32;

template <typename T>
T readLE(const uint8_t* p)
{
    T v = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
        v |= (T)((T)p[i] << (8 * i));
    return v;
}

float readFloat(const uint8_t* p)
{
    const uint32_t bits = readLE<uint32_t>(p);
    float f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

bool fail(std::string* why, const std::string& reason)
{
    if (why)
        *why = reason;
    return false;
}
}

uint32_t hdmCrc32(const uint8_t* data, size_t size)
{
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i)
        {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            t[i] = c;
        }
        return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

bool parseHdm(const uint8_t* data, size_t size, HdmMesh* out, std::string* why)
{
    if (!data || size < kHeaderSize + 4)
        return fail(why, "file of " + std::to_string(size) + " bytes is too short");
    if (std::memcmp(data, "AHDM", 4) != 0)
        return fail(why, "not an .hdm file (bad magic)");
    const uint16_t version = readLE<uint16_t>(data + 4);
    if (version != kHdmVersion)
        return fail(why, "unsupported version " + std::to_string(version));
    if (data[29] || data[30] || data[31])
        return fail(why, "reserved header bytes are not zero");
    const uint16_t groups = readLE<uint16_t>(data + 6);
    const uint64_t hash = readLE<uint64_t>(data + 8);
    const uint32_t nv = readLE<uint32_t>(data + 16);
    const uint32_t ni = readLE<uint32_t>(data + 20);
    const uint32_t nt = readLE<uint32_t>(data + 24);
    const uint8_t kind = data[28];
    if (nv > kHdmMaxVertices || ni > 3 * kHdmMaxTriangles || nt > kHdmMaxTextureBytes)
        return fail(why, "counts over budget");
    const size_t expected = kHeaderSize + (size_t)nv * sizeof(HdmVertex) + 4 * (size_t)ni + nt + 4;
    if (size != expected)
        return fail(why, "file is " + std::to_string(size) + " bytes, header says " + std::to_string(expected));
    if (readLE<uint32_t>(data + size - 4) != hdmCrc32(data, size - 4))
        return fail(why, "CRC mismatch");

    // The same rules, in the same order, as hdm.py check().
    if (groups < 1 || groups > 32)
        return fail(why, "group count " + std::to_string(groups) + " outside 1..32");
    if (nv < 3)
        return fail(why, "vertex count " + std::to_string(nv) + " outside 3.." + std::to_string(kHdmMaxVertices));
    if (ni == 0 || ni % 3 != 0)
        return fail(why, "index count " + std::to_string(ni) + " is not 1.." + std::to_string(kHdmMaxTriangles) + " triangles");
    if (kind != kHdmTexturePng && kind != kHdmTextureJpeg)
        return fail(why, "unknown texture kind " + std::to_string(kind));
    if (nt == 0)
        return fail(why, "texture of 0 bytes");

    HdmMesh mesh;
    mesh.groupCount = groups;
    mesh.skeletonHash = hash;
    mesh.textureKind = kind;
    mesh.vertices.resize(nv);
    const uint8_t* p = data + kHeaderSize;
    for (uint32_t v = 0; v < nv; ++v, p += sizeof(HdmVertex))
    {
        HdmVertex& dst = mesh.vertices[v];
        for (int k = 0; k < 3; ++k)
        {
            dst.position[k] = readFloat(p + 4 * k);
            dst.normal[k] = readFloat(p + 12 + 4 * k);
        }
        dst.uv[0] = readFloat(p + 24);
        dst.uv[1] = readFloat(p + 28);
        std::memcpy(dst.joints, p + 32, 4);
        std::memcpy(dst.weights, p + 36, 4);
    }
    mesh.indices.resize(ni);
    for (uint32_t i = 0; i < ni; ++i, p += 4)
        mesh.indices[i] = readLE<uint32_t>(p);
    mesh.texture.assign(p, p + nt);

    uint32_t maxIndex = 0;
    for (uint32_t i : mesh.indices)
        maxIndex = i > maxIndex ? i : maxIndex;
    if (maxIndex >= nv)
        return fail(why, "index " + std::to_string(maxIndex) + " >= vertex count " + std::to_string(nv));
    int maxJoint = 0;
    for (const HdmVertex& v : mesh.vertices)
        for (uint8_t j : v.joints)
            maxJoint = j > maxJoint ? j : maxJoint;
    if (maxJoint >= groups)
        return fail(why, "joint " + std::to_string(maxJoint) + " >= group count " + std::to_string(groups));
    for (uint32_t v = 0; v < nv; ++v)
    {
        const uint8_t* w = mesh.vertices[v].weights;
        const int sum = w[0] + w[1] + w[2] + w[3];
        if (sum != 255)
            return fail(why, "vertex " + std::to_string(v) + ": weights sum to " + std::to_string(sum) + ", not 255");
    }
    const char* fields[] = {"position", "normal", "uv"};
    for (int f = 0; f < 3; ++f)
        for (const HdmVertex& v : mesh.vertices)
        {
            const float* x = f == 0 ? v.position : f == 1 ? v.normal : v.uv;
            for (int k = 0; k < (f == 2 ? 2 : 3); ++k)
                if (!std::isfinite(x[k]))
                    return fail(why, std::string("non-finite ") + fields[f]);
        }
    if (out)
        *out = std::move(mesh);
    return true;
}

} // namespace models
