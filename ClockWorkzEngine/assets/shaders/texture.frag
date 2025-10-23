#version 330 core

in vec2 TexCoord;

out vec4 FragColor;

uniform sampler2D image;
uniform vec3 spriteColor;

void main()
{
    vec4 texColor = texture(image, TexCoord);

    // Multiply texture color by tint color (useful for fading, color mod, etc.)
    FragColor = vec4(spriteColor, 1.0) * texColor;

    // Optional: discard transparent pixels for better blending on cutout sprites
    // if (FragColor.a < 0.1) discard;
}
