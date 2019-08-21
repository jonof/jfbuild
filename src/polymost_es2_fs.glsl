#version 100

precision lowp float;

uniform sampler2D u_texture;
uniform sampler2D u_glowtexture;
uniform float u_alphacut;
uniform vec4 u_colour;
uniform vec4 u_fogcolour;
uniform float u_fogdensity;

varying vec4 v_colour;
varying mediump vec2 v_texcoord;

vec4 applyfog(vec4 inputcolour) {
    const float LOG2_E = 1.442695;
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float densdist = u_fogdensity * dist;
    float amount = 1.0 - clamp(exp2(-densdist * densdist * LOG2_E), 0.0, 1.0);
    //float amount = 1.0 - clamp(exp(-densdist), 0.0, 1.0);
    return mix(inputcolour, u_fogcolour, amount);
}

void main(void)
{
    vec4 texcolour;
    vec4 glowcolour;

    texcolour = texture2D(u_texture, v_texcoord);
    glowcolour = texture2D(u_glowtexture, v_texcoord);

    if (texcolour.a < u_alphacut) {
        discard;
    }

    texcolour = applyfog(texcolour);
    gl_FragColor = mix(texcolour * v_colour * u_colour, glowcolour, glowcolour.a);
}
