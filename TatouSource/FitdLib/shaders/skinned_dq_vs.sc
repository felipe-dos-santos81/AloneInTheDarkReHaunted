$input a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_texcoord0, v_normal, v_view

// HD character models: dual-quaternion skinning into the engine's camera
// space (models/dualQuat.h; no bone zooms, else skinned_vs.sc draws the body),
// then the classic projection exactly as skinned_vs.sc: after the divide x and
// y land on the same 320x200 pixel and depth is z / 40960.
#include "bgfx_shader.sh"

uniform vec4 u_camProj; // models::projParams: fovX/160, fovY/100, (centerX+shakeX)/160-1, 1-(centerY+shakeY)/100
uniform vec4 u_boneDq[64]; // per bone: rotation (x, y, z, w), then its dual part

void main()
{
    vec4 r0 = u_boneDq[int(a_indices.x) * 2];
    vec4 r1 = u_boneDq[int(a_indices.y) * 2];
    vec4 r2 = u_boneDq[int(a_indices.z) * 2];
    vec4 r3 = u_boneDq[int(a_indices.w) * 2];
    // each bone's sign aligned to the first: q and -q are one rotation
    float w1 = dot(r1, r0) < 0.0 ? -a_weight.y : a_weight.y;
    float w2 = dot(r2, r0) < 0.0 ? -a_weight.z : a_weight.z;
    float w3 = dot(r3, r0) < 0.0 ? -a_weight.w : a_weight.w;
    vec4 r = a_weight.x * r0 + w1 * r1 + w2 * r2 + w3 * r3;
    vec4 d = a_weight.x * u_boneDq[int(a_indices.x) * 2 + 1] + w1 * u_boneDq[int(a_indices.y) * 2 + 1]
           + w2 * u_boneDq[int(a_indices.z) * 2 + 1] + w3 * u_boneDq[int(a_indices.w) * 2 + 1];
    float len = length(r);
    r /= len;
    d /= len;
    vec3 rotated = a_position + 2.0 * cross(r.xyz, cross(r.xyz, a_position) + r.w * a_position);
    vec3 view = rotated + 2.0 * (r.w * d.xyz - d.w * r.xyz + cross(r.xyz, d.xyz));
    float z = view.z;
    gl_Position = vec4(view.x * u_camProj.x + u_camProj.z * z, -view.y * u_camProj.y + u_camProj.w * z, z * z / 40960.0, z);
    v_normal = a_normal + 2.0 * cross(r.xyz, cross(r.xyz, a_normal) + r.w * a_normal);
    v_view = view;
    v_texcoord0 = a_texcoord0;
}
