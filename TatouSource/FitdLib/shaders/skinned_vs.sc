$input a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_texcoord0, v_normal, v_view

// HD character models: four-bone skinning into the engine's camera space,
// then the classic projection (models::clipFromView): after the divide x and y
// land on the same 320x200 pixel as flat_vs.sc and depth is z / 40960, so HD
// and classic bodies, masks, SSAO and SSGI share one depth buffer.
#include "bgfx_shader.sh"

uniform vec4 u_camProj; // models::projParams: fovX/160, fovY/100, (centerX+shakeX)/160-1, 1-(centerY+shakeY)/100

void main()
{
    mat4 skin = a_weight.x * u_model[int(a_indices.x)]
              + a_weight.y * u_model[int(a_indices.y)]
              + a_weight.z * u_model[int(a_indices.z)]
              + a_weight.w * u_model[int(a_indices.w)];
    vec3 view = mul(skin, vec4(a_position, 1.0)).xyz;
    float z = view.z;
    gl_Position = vec4(view.x * u_camProj.x + u_camProj.z * z, -view.y * u_camProj.y + u_camProj.w * z, z * z / 40960.0, z);
    v_normal = mul(skin, vec4(a_normal, 0.0)).xyz;
    v_view = view;
    v_texcoord0 = a_texcoord0;
}
