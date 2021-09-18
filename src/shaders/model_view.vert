#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

layout (location = 0) in vec3 aPos;

out vec3 Position;

uniform mat4 mvp;

void main()
{
  gl_Position = mvp * vec4(aPos.x, aPos.y * 255.0, aPos.z, 1.0);
  Position = aPos;
}
