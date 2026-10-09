///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// Engine unit tests: the HD body file reader (models/hdmMesh). The fixture
// is shared with tests/tools/test_models_hdm.py, which checks that the
// importer's writer reproduces it byte for byte.
///////////////////////////////////////////////////////////////////////////////

#include "hdmMesh.h"
#include "doctest.h"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>

using namespace models;

namespace
{
// Offsets in tiny.hdm: 32-byte header, 4 x 40-byte vertices, 6 x u32
// indices, a 69-byte PNG, CRC.
constexpr size_t kVerts = 32, kIndices = 32 + 4 * 40, kTexture = kIndices + 6 * 4;

std::vector<uint8_t> tinyHdm()
{
    std::ifstream in(FITD_TEST_FIXTURES "/tiny.hdm", std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in), {});
}

// Recompute the trailing CRC so a test reaches the checks after it.
void resign(std::vector<uint8_t>& d)
{
    const uint32_t crc = hdmCrc32(d.data(), d.size() - 4);
    for (size_t i = 0; i < 4; ++i)
        d[d.size() - 4 + i] = (uint8_t)(crc >> (8 * i));
}

// `d` without `size` bytes at `offset` (re-sign with put() after).
void cut(std::vector<uint8_t>& d, size_t offset, size_t size)
{
    d.erase(d.begin() + (std::ptrdiff_t)offset, d.begin() + (std::ptrdiff_t)(offset + size));
}

template <typename T>
void put(std::vector<uint8_t>& d, size_t offset, T value)
{
    std::memcpy(d.data() + offset, &value, sizeof value); // the tests run on little-endian hosts
    resign(d);
}

std::string reason(const std::vector<uint8_t>& d)
{
    std::string why;
    HdmMesh mesh;
    CHECK_FALSE(parseHdm(d.data(), d.size(), &mesh, &why));
    return why;
}
}

TEST_CASE("hdm: the shared fixture parses to the values the importer wrote")
{
    const std::vector<uint8_t> d = tinyHdm();
    REQUIRE(d.size() == 289);
    HdmMesh mesh;
    std::string why;
    REQUIRE_MESSAGE(parseHdm(d.data(), d.size(), &mesh, &why), why);
    CHECK(mesh.groupCount == 2);
    CHECK(mesh.skeletonHash == 0x0123456789ABCDEFull);
    REQUIRE(mesh.vertices.size() == 4);
    CHECK(mesh.vertices[3].position[0] == 100.0f);
    CHECK(mesh.vertices[3].position[1] == -200.0f);
    CHECK(mesh.vertices[3].position[2] == 50.0f);
    CHECK(mesh.vertices[1].normal[2] == -1.0f);
    CHECK(mesh.vertices[3].uv[0] == 1.0f);
    CHECK(mesh.vertices[1].joints[1] == 1);
    CHECK(mesh.vertices[1].weights[0] == 128);
    CHECK(mesh.vertices[1].weights[1] == 127);
    CHECK(mesh.indices == std::vector<uint32_t>{0, 1, 2, 2, 1, 3});
    CHECK(mesh.textureKind == kHdmTexturePng);
    CHECK_FALSE(mesh.translucent);
    REQUIRE(mesh.texture.size() == 69);
    CHECK(mesh.texture[1] == 'P');
}

TEST_CASE("hdm: the translucent flag reads back")
{
    std::vector<uint8_t> d = tinyHdm();
    put<uint8_t>(d, 29, kHdmFlagTranslucent);
    HdmMesh mesh;
    std::string why;
    REQUIRE_MESSAGE(parseHdm(d.data(), d.size(), &mesh, &why), why);
    CHECK(mesh.translucent);
}

TEST_CASE("hdm: CRC-32 is zlib's")
{
    const char* text = "123456789";
    CHECK(hdmCrc32((const uint8_t*)text, 9) == 0xCBF43926u);
}

TEST_CASE("hdm: every broken rule is refused with its reason")
{
    const std::vector<uint8_t> good = tinyHdm();
    REQUIRE(good.size() == 289);
    auto d = good;

    d.resize(30);
    CHECK(reason(d) == "file of 30 bytes is too short");

    d = good;
    d[0] = 'X';
    resign(d);
    CHECK(reason(d) == "not an .hdm file (bad magic)");

    d = good;
    put<uint16_t>(d, 4, 2);
    CHECK(reason(d) == "unsupported version 2");

    d = good;
    put<uint8_t>(d, 29, 2); // a flag bit with no meaning
    CHECK(reason(d) == "reserved header bytes are not zero");

    d = good;
    d.insert(d.end() - 4, 0);
    resign(d);
    CHECK(reason(d) == "file is 290 bytes, header says 289");

    d = good;
    d[kTexture] ^= 1;
    CHECK(reason(d) == "CRC mismatch");

    d = good;
    put<uint16_t>(d, 6, 0);
    CHECK(reason(d) == "group count 0 outside 1..32");

    d = good;
    put<uint16_t>(d, 6, 33);
    CHECK(reason(d) == "group count 33 outside 1..32");

    d = good;
    put<uint8_t>(d, 28, 3);
    CHECK(reason(d) == "unknown texture kind 3");

    d = good;
    put<uint32_t>(d, kIndices + 20, 4);
    CHECK(reason(d) == "index 4 >= vertex count 4");

    d = good;
    put<uint8_t>(d, kVerts + 2 * 40 + 32, 2);
    CHECK(reason(d) == "joint 2 >= group count 2");

    d = good;
    put<uint8_t>(d, kVerts + 40 + 36, 127);
    CHECK(reason(d) == "vertex 1: weights sum to 254, not 255");

    d = good;
    put<float>(d, kVerts, std::numeric_limits<float>::quiet_NaN());
    CHECK(reason(d) == "non-finite position");

    d = good;
    put<float>(d, kVerts + 3 * 40 + 12, std::numeric_limits<float>::infinity());
    CHECK(reason(d) == "non-finite normal");

    d = good;
    put<float>(d, kVerts + 24, std::numeric_limits<float>::quiet_NaN());
    CHECK(reason(d) == "non-finite uv");

    d = good;
    put<uint32_t>(d, 16, 150001);
    CHECK(reason(d) == "counts over budget");

    d = good;
    cut(d, kVerts + 2 * 40, 2 * 40);
    put<uint32_t>(d, 16, 2);
    CHECK(reason(d) == "vertex count 2 outside 3..150000");

    d = good;
    cut(d, kIndices + 20, 4);
    put<uint32_t>(d, 20, 5);
    CHECK(reason(d) == "index count 5 is not 1..50000 triangles");

    d = good;
    cut(d, kTexture, 69);
    put<uint32_t>(d, 24, 0);
    CHECK(reason(d) == "texture of 0 bytes");
}

TEST_CASE("hdm: a null buffer is refused, and *why is optional")
{
    CHECK_FALSE(parseHdm(nullptr, 0, nullptr, nullptr));
    const std::vector<uint8_t> d = tinyHdm();
    CHECK(parseHdm(d.data(), d.size(), nullptr, nullptr));
}
