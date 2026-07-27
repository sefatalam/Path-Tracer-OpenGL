#version 460 core

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D screenTexture;
uniform float exposure;

// Narkowicz's fit of the ACES filmic curve.
// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
vec3 acesTonemap(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    // Apply exposure + tonemapping + gamma correction
    vec3 color = texture(screenTexture, TexCoord).rgb * exposure;
    color = acesTonemap(color);
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
