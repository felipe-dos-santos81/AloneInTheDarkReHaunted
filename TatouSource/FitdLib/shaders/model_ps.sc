$input v_texcoord0, v_normal, v_view

// HD character models: the base colour, darkened with the room (a dark room
// scales the palette to 10 %) and the fade, as the classic body is. Stage S4
// adds the lighting.
#include "bgfx_shader.sh"

SAMPLER2D(s_albedo, 0);
uniform vec4 u_tint; // x: fade level (0..1), y: room brightness (1, or 0.1 in a dark room)

void main()
{
    if (v_view.z < 100.0)
        discard; // the classic path drops what is this near the lens
    vec4 albedo = texture2D(s_albedo, v_texcoord0);
    if (albedo.a < 0.5)
        discard;
    gl_FragColor = vec4(albedo.rgb * (u_tint.x * u_tint.y), 1.0);
}
