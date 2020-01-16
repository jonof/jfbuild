#version 100

precision lowp float;
precision lowp int;
precision lowp sampler2D;

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
        gl_FragColor = mix(u_bgcolour, u_colour, pixel.a);
    } else if (u_mode == 1) {
        // Tile screen.
        pixel = texture2D(u_texture, v_texcoord);
        gl_FragColor = mix(u_bgcolour, pixel, pixel.a);
    } else if (u_mode == 2) {
        // Foreground colour.
        gl_FragColor = u_colour;
    }
}
