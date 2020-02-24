#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 Position;

void main()
{
  float offset = 500.0f;
  gl_Position = vec4((aPos.x - offset) / offset, (aPos.y - offset) / offset, aPos.z, 1.0); 
  Position = gl_Position.xyz;
}
