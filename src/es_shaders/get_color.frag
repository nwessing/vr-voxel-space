#version 310 es
out lowp vec4 FragColor;

in lowp vec3 Position;

uniform sampler2D colorMap;

void main()
{ 
  lowp ivec2 tex_size = textureSize(colorMap, 0);
  lowp vec2 tex_sizef = vec2(tex_size.x, tex_size.y);
  FragColor = texture(colorMap, Position.xy / tex_sizef);
}
