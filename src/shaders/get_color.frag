#version 330 core
out vec4 FragColor;

in vec3 Position;

uniform sampler2D colorMap;

void main()
{ 
  ivec2 tex_size = textureSize(colorMap, 0);
  vec2 floored = vec2(ceil(Position.x), ceil(Position.y));
  FragColor = texture(colorMap, Position.xy / tex_size);
}
