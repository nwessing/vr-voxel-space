#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

layout (location = 0) in vec2 aPos;

out vec2 TexCoords;

void main()
{
  gl_Position = vec4(aPos.x, - aPos.y, 0.0, 1.0);
  TexCoords = vec2((aPos.x + 1.0) / 2.0, (aPos.y + 1.0) / 2.0);
}
