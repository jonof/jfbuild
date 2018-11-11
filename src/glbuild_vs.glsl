#glbuild(ES2) #version 100
#glbuild(2)   #version 110
#glbuild(3)   #version 140

#ifdef GL_ES
#elif __VERSION__ < 140
#  define mediump
#else
#  define attribute in
#  define varying out
#endif

attribute mediump vec2 a_vertex;
attribute mediump vec2 a_texcoord;
varying mediump vec2 v_texcoord;

void main(void)
{
  v_texcoord = a_texcoord;
  gl_Position = vec4(a_vertex, 0.0, 1.0);
}
