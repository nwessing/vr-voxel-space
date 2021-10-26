#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

layout (location = 0) in vec3 aPos;

out vec3 Position;
out vec4 WorldPosition;
out float CameraDistance;

uniform vec3 cameraPosition;

// Per instance uniforms
uniform mat4 mvp;
uniform mat4 model;

void main()
{
  gl_Position = mvp * vec4(aPos.x, aPos.y, aPos.z, 1.0);
  WorldPosition = vec4(gl_Position);
  CameraDistance = distance(cameraPosition, vec3(model * vec4(aPos, 1.0)));
  Position = aPos;
}
