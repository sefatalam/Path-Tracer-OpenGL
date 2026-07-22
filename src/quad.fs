#version 460 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D screenTexture;
void main()
{
    // vec3 texCol = texture(screenTexture, TexCoord).rgb;
    // FragColor = vec4(texCol,1.0);

    FragColor = texture(screenTexture, TexCoord);
}