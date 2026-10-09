///////////////////////////////////////////////////////////////////////////////
// Alone In The Dark Re-Haunted
// HD character models: loading, caching and drawing replacement meshes.
///////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "modelReplacement.h"

#include <bgfx/bgfx.h>

#include <cstdio>
#include <string>
#include <unordered_map>
#include <vector>

#include "configRemaster.h"
#include "consoleLog.h"
#include "hdCompare.h"
#include "lanternLighting.h"
#include "models/bodyPose.h"
#include "models/hdmMesh.h"
#include "models/mipChain.h"
#include "models/modelLight.h"
#include "models/renderCamera.h"
#include "models/replacementGate.h"
#include "models/skinnedBody.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include "../ThirdParty/bgfx.cmake/bimg/3rdparty/stb/stb_image.h"

extern "C" {
    extern char homePath[512];
}
extern unsigned int gameViewId;
extern bgfx::ProgramHandle loadBgfxProgram(const std::string& VSFile, const std::string& PSFile);

#define HDM_WARN CON_MAKE_WARN("HDM")

namespace
{
constexpr float kDarkRoomBrightness = 0.10f; // rendererBGFX.cpp: DARK_ROOM_BRIGHTNESS

struct GpuVertex
{
    float position[3];
    float normal[3];
    float uv[2];
    float joints[4];    // as floats: no integer attributes on Metal or SPIR-V
    uint8_t weights[4]; // normalised: sum 255
};
}

struct ModelReplacement
{
    enum class State { Missing, Ready, Rejected } state = State::Missing;
    std::string key; // LISTBODY_011
    models::PoseBody pose;
    models::SkinBind skin;
    bgfx::VertexBufferHandle vb = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ib = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle texture = BGFX_INVALID_HANDLE;
    uint32_t textureFlags = 0;
    bool translucent = false; // some texels are the transparent material: a second, blended pass
};

namespace
{
std::unordered_map<std::string, ModelReplacement> s_cache;
int s_loads = 0;
bool s_boxValid = false;
int s_box[4] = { 0, 0, 0, 0 };

// Created once. An invalid program (a backend whose shaders were not built)
// is reported once; every body then draws classic instead of vanishing.
bgfx::ProgramHandle modelProgram()
{
    static bool tried = false;
    static bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
    if (!tried)
    {
        tried = true;
        program = loadBgfxProgram("skinned_vs", "model_ps");
        if (!bgfx::isValid(program))
        {
            printf(HDM_WARN "the HD model shaders are not available on this renderer: drawing classic bodies" CON_RESET "\n");
            fflush(stdout);
        }
    }
    return program;
}

bgfx::UniformHandle uniform(const char* name, bgfx::UniformType::Enum type)
{
    static std::unordered_map<std::string, bgfx::UniformHandle> handles;
    auto it = handles.find(name);
    if (it == handles.end())
        it = handles.emplace(name, bgfx::createUniform(name, type)).first;
    return it->second;
}

models::PoseBody poseBodyOf(const sBody* body)
{
    models::PoseBody p;
    p.flags = body->m_flags;
    for (const point3dStruct& v : body->m_vertices)
        p.verts.push_back({ v.x, v.y, v.z });
    for (const sGroup& g : body->m_groups)
        p.groups.push_back({ g.m_start, g.m_numVertices, g.m_baseVertices, g.m_orgGroup, g.m_numGroup });
    for (uint16 o : body->m_groupOrder)
        p.order.push_back(o);
    return p;
}

// The planar shadows' direction (vars.cpp), so shading and shadows agree.
void setLightUniforms(const models::RenderCamera& cam)
{
    const models::Vec3 key = models::cameraDirection(cam, { g_shadowLightDirX, g_shadowLightDirY, g_shadowLightDirZ });
    const float keyLight[4] = { key.x, key.y, key.z, models::kKeyStrength };
    bgfx::setUniform(uniform("u_keyLight", bgfx::UniformType::Vec4), keyLight);
    const models::Vec3 up = models::cameraDirection(cam, { 0.0f, -1.0f, 0.0f }); // the engine's y points down
    const float ambient[4] = { up.x, up.y, up.z, 0.0f };
    bgfx::setUniform(uniform("u_ambient", bgfx::UniformType::Vec4), ambient);
    const float levels[4] = { models::kAmbientGround, models::kAmbientSky, models::kSpecular, models::kShininess };
    bgfx::setUniform(uniform("u_ambientLevels", bgfx::UniformType::Vec4), levels);
    float pos[3], colour[3], intensity = 0.0f;
    float lantern[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, lanternColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (heldLanternLight(pos, colour, &intensity))
    {
        const models::Vec3 at = models::cameraPoint(cam, models::lanternHandPoint({ pos[0], pos[1], pos[2] }));
        lantern[0] = at.x;
        lantern[1] = at.y;
        lantern[2] = at.z;
        lantern[3] = models::kLanternReach;
        for (int k = 0; k < 3; ++k)
            lanternColour[k] = colour[k] * intensity;
    }
    bgfx::setUniform(uniform("u_lantern", bgfx::UniformType::Vec4), lantern);
    bgfx::setUniform(uniform("u_lanternColour", bgfx::UniformType::Vec4), lanternColour);
}

models::RenderCamera engineCamera()
{
    models::RenderCamera c;
    c.alpha = transformX;
    c.beta = transformY;
    c.gamma = transformZ;
    c.posX = translateX;
    c.posY = translateY;
    c.posZ = translateZ;
    c.persp = cameraPerspective;
    c.fovX = cameraFovX;
    c.fovY = cameraFovY;
    c.centerX = cameraCenterX;
    c.centerY = cameraCenterY;
    c.table = cosTable;
    return c;
}

bool readFile(const std::string& path, std::vector<uint8_t>* out)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;
    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    out->resize(size > 0 ? (size_t)size : 0);
    const bool ok = size > 0 && fread(out->data(), 1, out->size(), f) == out->size();
    fclose(f);
    return ok;
}

bool upload(ModelReplacement& r, const models::HdmMesh& mesh, std::string* why)
{
    int w = 0, h = 0, channels = 0;
    stbi_uc* rgba = stbi_load_from_memory(mesh.texture.data(), (int)mesh.texture.size(), &w, &h, &channels, 4);
    if (!rgba)
    {
        *why = std::string("texture does not decode: ") + stbi_failure_reason();
        return false;
    }
    r.translucent = models::hasTranslucentTexels(rgba, w, h);
    const bool swatch = w <= models::kSwatchSide && h <= models::kSwatchSide;
    if (swatch)
    {
        r.texture = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, false, 1, bgfx::TextureFormat::RGBA8, 0,
                                          bgfx::copy(rgba, (uint32_t)(w * h * 4)));
        r.textureFlags = BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP;
    }
    else
    {
        int levels = 0;
        const std::vector<uint8_t> chain = models::rgba8MipChain(rgba, w, h, &levels);
        r.texture = bgfx::createTexture2D((uint16_t)w, (uint16_t)h, true, 1, bgfx::TextureFormat::RGBA8, 0,
                                          bgfx::copy(chain.data(), (uint32_t)chain.size()));
        r.textureFlags = 0;
    }
    stbi_image_free(rgba);

    std::vector<GpuVertex> vertices(mesh.vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const models::HdmVertex& v = mesh.vertices[i];
        GpuVertex& g = vertices[i];
        for (int k = 0; k < 3; ++k)
        {
            g.position[k] = v.position[k];
            g.normal[k] = v.normal[k];
        }
        g.uv[0] = v.uv[0];
        g.uv[1] = v.uv[1];
        for (int k = 0; k < 4; ++k)
        {
            g.joints[k] = (float)v.joints[k];
            g.weights[k] = v.weights[k];
        }
    }
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Indices, 4, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Weight, 4, bgfx::AttribType::Uint8, true)
        .end();
    r.vb = bgfx::createVertexBuffer(bgfx::copy(vertices.data(), (uint32_t)(vertices.size() * sizeof(GpuVertex))), layout);
    r.ib = bgfx::createIndexBuffer(bgfx::copy(mesh.indices.data(), (uint32_t)(mesh.indices.size() * 4)),
                                   BGFX_BUFFER_INDEX32);
    if (!bgfx::isValid(r.texture) || !bgfx::isValid(r.vb) || !bgfx::isValid(r.ib))
    {
        *why = "GPU resources could not be created";
        return false;
    }
    return true;
}

void destroy(ModelReplacement& r)
{
    if (bgfx::isValid(r.vb))
        bgfx::destroy(r.vb);
    if (bgfx::isValid(r.ib))
        bgfx::destroy(r.ib);
    if (bgfx::isValid(r.texture))
        bgfx::destroy(r.texture);
    r.vb = BGFX_INVALID_HANDLE;
    r.ib = BGFX_INVALID_HANDLE;
    r.texture = BGFX_INVALID_HANDLE;
}

void load(ModelReplacement& r, const std::string& key, const sBody* body)
{
    const std::string path = std::string(homePath) + "models_hd/body_" + key + ".hdm";
    std::vector<uint8_t> bytes;
    if (!readFile(path, &bytes))
        return; // no replacement for this body: draw classic, quietly
    ++s_loads;
    std::string why;
    models::HdmMesh mesh;
    r.pose = poseBodyOf(body);
    if (!models::validateSkeleton(r.pose, &why) || !models::parseHdm(bytes.data(), bytes.size(), &mesh, &why) ||
        !models::prepareSkin(r.pose, mesh, cosTable, &r.skin, &why) || !upload(r, mesh, &why))
    {
        destroy(r);
        r.state = ModelReplacement::State::Rejected;
        printf(HDM_WARN "%s rejected, drawing the classic body: %s" CON_RESET "\n", path.c_str(), why.c_str());
        fflush(stdout);
        return;
    }
    r.state = ModelReplacement::State::Ready;
    printf("[HDM] loaded %s: %zu vertices, %zu triangles\n", path.c_str(), mesh.vertices.size(), mesh.indices.size() / 3);
    fflush(stdout);
}
}

bool hdModelsActive()
{
    return g_remasterConfig.graphics.enableHDModels && g_gameId == AITD1 && !hdCompareForcesClassic();
}

ModelReplacement* findModelReplacement(int bodyNum, sBody* pBody, const std::string& hqrName)
{
    if (!hdModelsActive() || !pBody || !(pBody->m_flags & models::kInfoAnim) || (pBody->m_flags & models::kInfoOptimise))
        return nullptr;
    char key[64];
    snprintf(key, sizeof key, "%s_%03d", hqrName.c_str(), bodyNum);
    auto it = s_cache.find(key);
    if (it == s_cache.end())
    {
        it = s_cache.emplace(key, ModelReplacement{}).first;
        it->second.key = key;
        load(it->second, key, pBody);
    }
    return it->second.state == ModelReplacement::State::Ready ? &it->second : nullptr;
}

bool drawModelReplacement(ModelReplacement* r, sBody* pBody, int x, int y, int z, int alpha, int beta, int gamma)
{
    const size_t groups = r->pose.groups.size();
    if (pBody->m_groups.size() != groups)
        return false;
    std::vector<models::GroupState> states(groups);
    for (size_t g = 0; g < groups; ++g)
    {
        const sGroupState& s = pBody->m_groups[g].m_state;
        states[g] = { s.m_type, s.m_delta.x, s.m_delta.y, s.m_delta.z };
    }
    const models::RenderCamera cam = engineCamera();
    models::Affine3 bones[models::kMaxPoseGroups];
    const bool poseOk = models::boneMatrices(r->pose, states.data(), alpha, beta, gamma, x, y, z, cam, r->skin, bones);
    const models::GateInput gate{ hdModelsActive(), g_gameId == AITD1, true, (pBody->m_flags & models::kInfoAnim) != 0,
                                  (pBody->m_flags & models::kInfoOptimise) != 0, r->state == ModelReplacement::State::Ready,
                                  poseOk, bgfx::isValid(modelProgram()) };
    if (!models::drawReplacement(gate))
        return false;

    osystem_flushPendingPrimitives(); // keep submission order: the view is sequential
    const models::ProjParams p = models::projParams(cam, g_shakeOffsetX, g_shakeOffsetY);
    s_boxValid = models::screenBox(bones, r->skin, p, s_box);
    if (s_boxValid)
        hdCompareNoteDraw(r->key.c_str(), s_box);
    if (hdCompareHidden())
        return true; // compare mode's hidden frame: neither the replacement nor the classic body

    float matrices[models::kMaxPoseGroups][16];
    for (size_t g = 0; g < groups; ++g)
        models::columnMajor(bones[g], matrices[g]);
    const float proj[4] = { p.px, p.py, p.pz, p.pw };
    // The opaque texels, then (as the classic path draws the transparent material:
    // blended, writing no depth, back faces culled; the stage faces every triangle
    // outward) the translucent ones. bgfx consumes the state per submit.
    for (int pass = 0; pass < (r->translucent ? 2 : 1); ++pass)
    {
        bgfx::setTransform(matrices, (uint16_t)groups);
        bgfx::setUniform(uniform("u_camProj", bgfx::UniformType::Vec4), proj);
        const float tint[4] = { g_fadeLevel, g_roomIsDark ? kDarkRoomBrightness : 1.0f, (float)pass,
                                hdCompareUnlit() ? 1.0f : 0.0f };
        bgfx::setUniform(uniform("u_tint", bgfx::UniformType::Vec4), tint);
        setLightUniforms(cam);
        bgfx::setTexture(0, uniform("s_albedo", bgfx::UniformType::Sampler), r->texture, r->textureFlags);
        bgfx::setVertexBuffer(0, r->vb);
        bgfx::setIndexBuffer(r->ib);
        bgfx::setState(pass == 0 ? BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z |
                                       BGFX_STATE_DEPTH_TEST_LEQUAL | BGFX_STATE_MSAA
                                 : BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL |
                                       BGFX_STATE_MSAA | BGFX_STATE_BLEND_ALPHA | BGFX_STATE_CULL_CW);
        bgfx::submit(gameViewId, modelProgram());
    }
    return true;
}

bool lastReplacementBox(int box[4])
{
    if (!s_boxValid)
        return false;
    for (int i = 0; i < 4; ++i)
        box[i] = s_box[i];
    return true;
}

void forgetReplacementBox()
{
    s_boxValid = false;
}

int modelReplacementLoads()
{
    return s_loads;
}

void shutdownModelReplacements()
{
    for (auto& entry : s_cache)
        destroy(entry.second);
    s_cache.clear();
}
