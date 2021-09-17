#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

layout (location = 0) in vec3 aPos;

out vec3 Position;

uniform mat4 view;
uniform mat4 projection;
uniform mat4 model;

void main()
{
  gl_Position = projection * view * model * vec4(aPos.xy, aPos.z * -255.0, 1.0);
  Position = aPos;
}
