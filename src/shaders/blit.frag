#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;

void main()
{ 
    vec4 texColor = texture(screenTexture, TexCoords);
    if (texColor.a < 0.5) {
      discard;
    }
    FragColor = texColor;
}
