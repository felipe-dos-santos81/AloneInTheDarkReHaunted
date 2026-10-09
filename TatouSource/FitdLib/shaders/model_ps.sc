$input v_texcoord0, v_normal, v_view

// HD character models: restrained shading in the engine camera's space. A
// hemisphere ambient and a Lambert key light from the planar-shadow direction
// (so shading agrees with the shadows), a mild Blinn highlight, and the held
// lantern as a point light. A dark room scales ambient and key light as the
// classic palette is scaled; the fade darkens everything. u_tint.w = 1 shows
// the unlit texture (compare mode). Texture alpha below 0.5 is a hole; from 0.5
// to 0.99 a translucent texel (the engine's transparent material), drawn only in
// the blended pass (u_tint.z = 1); the opaque pass (u_tint.z = 0) draws the rest.
#include "bgfx_shader.sh"

SAMPLER2D(s_albedo, 0);
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
    float translucent = albedo.a < 0.99 ? 1.0 : 0.0;
    if (abs(translucent - u_tint.z) > 0.5)
        discard; // the other pass draws this texel
    float alpha = translucent > 0.5 ? albedo.a : 1.0;
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
