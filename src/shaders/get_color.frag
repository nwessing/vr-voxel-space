#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

out vec4 FragColor;

in vec3 Position;

uniform sampler2D colorMap;
uniform ivec2 heightMapSize;
uniform vec3 blendColor;

void main()
{
  FragColor = texture(colorMap, Position.xz / vec2(heightMapSize)) * vec4(blendColor, 1.0);
}
