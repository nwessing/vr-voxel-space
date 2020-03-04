#version 310 es
out lowp vec4 FragColor;

in lowp vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{ 
    lowp vec4 texColor = texture(screenTexture, TexCoords);
    if (texColor.a < 0.5) {
      discard;
    }
    FragColor = texColor;
}
