#version 100

attribute vec3 a_vertex;
attribute mediump vec2 a_texcoord;
attribute lowp vec4 a_colour;

uniform mat4 u_modelview;
uniform mat4 u_projection;

varying lowp vec4 v_colour;
varying mediump vec2 v_texcoord;

void main(void)
{
    v_texcoord = a_texcoord;
    v_colour = a_colour;
    gl_Position = u_projection * u_modelview * vec4(a_vertex, 1.0);
}
