#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
precision lowp float;
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
uniform sampler2D u_glowtexture;
uniform vec4 u_colour;
uniform float u_alphacut;
uniform vec4 u_fogcolour;
uniform float u_fogdensity;

varying mediump vec2 v_texcoord;

vec4 applyfog(vec4 inputcolour) {
    const float LOG2_E = 1.442695;
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float densdist = u_fogdensity * dist;
    float amount = 1.0 - clamp(exp2(-densdist * densdist * LOG2_E), 0.0, 1.0);
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
    o_fragcolour = mix(texcolour * u_colour, glowcolour, glowcolour.a);
}
