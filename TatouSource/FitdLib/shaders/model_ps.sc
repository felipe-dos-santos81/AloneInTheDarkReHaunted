$input v_texcoord0, v_normal, v_view

// HD character models: restrained shading in the engine camera's space. A
// hemisphere ambient and a Lambert key light from the planar-shadow direction
// (so shading agrees with the shadows), a mild Blinn highlight, and the held
// lantern as a point light. A dark room scales ambient and key light as the
// classic palette is scaled; the fade darkens everything. u_tint.w = 1 shows
// the unlit texture (compare mode). Filtered texture alpha below 0.5 is a hole,
// so a cutout's edge stays smooth. Whether a texel is translucent (the engine's
// transparent material) comes from the same texture point-sampled, whose levels
// keep each texel's class (mipChain.h): no filtered edge turns translucent. The
// blended pass (u_tint.z = 1) draws the translucent texels, the opaque pass
// (u_tint.z = 0) the rest.
#include "bgfx_shader.sh"

SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_alphaClass, 1); // s_albedo's texture, point-sampled
uniform vec4 u_tint;       // x: fade (0..1), y: room brightness (1, or 0.1 dark), z: 1 = blended pass, w: 1 = unlit
uniform vec4 u_keyLight;   // xyz: direction to the key light (camera space), w: strength
uniform vec4 u_ambient;    // xyz: up (camera space), w: unused
uniform vec4 u_ambientLevels; // x: ground, y: sky, z: specular strength, w: shininess
uniform vec4 u_lantern;    // xyz: lantern position (camera space), w: reach (0 = no lantern)
uniform vec4 u_lanternColour; // rgb: colour x intensity

void main()
{
    if (v_view.z < 100.0)
        discard; // the classic path drops what is this near the lens
    vec4 albedo = texture2D(s_albedo, v_texcoord0);
    if (albedo.a < 0.5)
        discard;
    float own = texture2D(s_alphaClass, v_texcoord0).a;
    bool blended = own >= 127.5 / 255.0 && own < 252.5 / 255.0; // mipChain.h kTranslucentAlpha, kOpaqueAlpha
    if (blended != (u_tint.z > 0.5))
        discard; // the other pass draws this texel
    float alpha = blended ? own : 1.0;
    if (u_tint.w > 0.5)
    {
        gl_FragColor = vec4(albedo.rgb * (u_tint.x * u_tint.y), alpha);
        return;
    }
    vec3 n = normalize(v_normal);
    vec3 toEye = normalize(-v_view);
    float up = 0.5 + 0.5 * dot(n, u_ambient.xyz);
    vec3 light = vec3_splat(mix(u_ambientLevels.x, u_ambientLevels.y, up));
    float lambert = max(dot(n, u_keyLight.xyz), 0.0);
    light += vec3_splat(u_keyLight.w * lambert);
    vec3 halfway = normalize(u_keyLight.xyz + toEye);
    float highlight = lambert > 0.0 ? u_ambientLevels.z * pow(max(dot(n, halfway), 0.0), u_ambientLevels.w) : 0.0;
    vec3 colour = (albedo.rgb * light + vec3_splat(highlight)) * u_tint.y;
    if (u_lantern.w > 0.0)
    {
        vec3 toLantern = u_lantern.xyz - v_view;
        float d2 = dot(toLantern, toLantern) / (u_lantern.w * u_lantern.w);
        float falloff = max(1.0 - d2, 0.0);
        falloff *= falloff;
        colour += albedo.rgb * u_lanternColour.rgb * falloff * max(dot(n, normalize(toLantern)), 0.0);
    }
    gl_FragColor = vec4(colour * u_tint.x, alpha);
}
