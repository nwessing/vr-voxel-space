#ifdef OPENGL_ES
  precision highp float;
  precision highp int;
#endif

const uint ENABLE_FOG = 1u;

out vec4 FragColor;

in vec3 Position;
in vec4 WorldPosition;
in float CameraDistance;

uniform sampler2D colorMap;
uniform ivec2 heightMapSize;
uniform vec4 fogColor;
uniform float terrainScale;
uniform uint flags;

// Per instance uniforms
uniform vec4 blendColor;


float DISTANCE_FOG_MIN = 1500.0f;
float DISTANCE_FOG_MAX = 1850.0f;

void main()
{
  vec4 color = texture(colorMap, Position.xz / vec2(heightMapSize)) * vec4(blendColor);

  if ((flags & ENABLE_FOG) != 0u) {
    float distanceMin = DISTANCE_FOG_MIN * terrainScale;
    float distanceMax = DISTANCE_FOG_MAX * terrainScale;
    float fogFactor = (distanceMin - CameraDistance) / (distanceMin - distanceMax);
    fogFactor = clamp(fogFactor, 0.0, 1.0);
    FragColor = mix(color, fogColor, fogFactor);
  } else {
    FragColor = color;
  }
}
