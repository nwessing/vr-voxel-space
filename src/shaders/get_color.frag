#version 330 core
out vec4 FragColor;

in vec3 Position;

uniform sampler2D colorMap;

void main()
{ 
  // float color = Position.z;
  // FragColor = vec4(color, color, color, 1.0);
  FragColor = texture(colorMap, vec2(Position.x / 1024.0f, Position.y / 1024.0f));
}
