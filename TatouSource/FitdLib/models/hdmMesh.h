///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: the body_<KEY>.hdm file the importer writes
// (tools/aitd_models/hdm.py, docs/model-contract.md "Import output").
// Engine-free: standard headers only.
//
// Little-endian, no padding: a 32-byte header ("AHDM", u16 version 1, u16
// group count, u64 skeleton hash, u32 vertex count, u32 index count, u32
// texture bytes, u8 texture kind, 3 zero bytes), 40-byte vertices, u32
// indices, the texture bytes, and a CRC-32 of everything before it.
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace models
{

constexpr uint16_t kHdmVersion = 1;
constexpr uint32_t kHdmMaxTriangles = 50000;
constexpr uint32_t kHdmMaxVertices = 3 * kHdmMaxTriangles;
constexpr uint32_t kHdmMaxTextureBytes = 64u << 20;
constexpr uint8_t kHdmTexturePng = 1;
constexpr uint8_t kHdmTextureJpeg = 2;

// One vertex, in the file's order: engine space, rest pose; joints are bone
// groups and the four weights sum to 255.
struct HdmVertex
{
    float position[3];
    float normal[3];
    float uv[2];
    uint8_t joints[4];
    uint8_t weights[4];
};
static_assert(sizeof(HdmVertex) == 40, "HdmVertex must match the file's 40-byte record");

struct HdmMesh
{
    uint16_t groupCount = 0;
    uint64_t skeletonHash = 0;    // models::skeletonHash of the body it was made for
    std::vector<HdmVertex> vertices;
    std::vector<uint32_t> indices; // triangles
    std::vector<uint8_t> texture;  // PNG or JPEG bytes, as delivered
    uint8_t textureKind = 0;       // kHdmTexturePng or kHdmTextureJpeg
};

// Parses and validates a whole file. On failure returns false, leaves *out
// unspecified and, when given, sets *why to the reason (the same reasons, in
// the same order, as tools/aitd_models/hdm.py read_hdm).
bool parseHdm(const uint8_t* data, size_t size, HdmMesh* out, std::string* why);

// zlib's CRC-32 (polynomial 0xEDB88320), as the importer writes it.
uint32_t hdmCrc32(const uint8_t* data, size_t size);

} // namespace models
