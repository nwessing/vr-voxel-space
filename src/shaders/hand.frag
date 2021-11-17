#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

out vec4 FragColor;

in vec3 Position;
in vec2 Uv;

uniform sampler2D colorMap;

void main() {
  FragColor = texture(colorMap, Uv);
}
