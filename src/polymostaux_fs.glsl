#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
precision lowp float;
precision lowp int;
#  define o_fragcolour gl_FragColor
#elif __VERSION__ < 140
#  define mediump
#  define o_fragcolour gl_FragColor
#else
#  define varying in
#  define texture2D texture
out vec4 o_fragcolour;
#endif

uniform sampler2D u_texture;
uniform vec4 u_colour;
uniform vec4 u_bgcolour;
uniform int u_mode;

varying mediump vec2 v_texcoord;

void main(void)
{
    vec4 pixel;

    if (u_mode == 0) {
        // Text.
        pixel = texture2D(u_texture, v_texcoord);
        o_fragcolour = mix(u_bgcolour, u_colour, pixel.a);
    } else if (u_mode == 1) {
        // Tile screen.
        pixel = texture2D(u_texture, v_texcoord);
        o_fragcolour = mix(u_bgcolour, pixel, pixel.a);
    } else if (u_mode == 2) {
        // Foreground colour.
        o_fragcolour = u_colour;
    }
}
