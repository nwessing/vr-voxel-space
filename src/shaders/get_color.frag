#version 330 core
out vec4 FragColor;

in vec3 Position;

// uniform sampler2D screenTexture;

void main()
{ 
  float color = Position.z;
  FragColor = vec4(color, color, color, 1.0);
}
