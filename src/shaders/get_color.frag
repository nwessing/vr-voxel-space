#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

out vec4 FragColor;

in vec3 Position;

uniform sampler2D colorMap;

void main()
{
  ivec2 tex_size = textureSize(colorMap, 0);
  FragColor = texture(colorMap, Position.xz / vec2(tex_size));
}
