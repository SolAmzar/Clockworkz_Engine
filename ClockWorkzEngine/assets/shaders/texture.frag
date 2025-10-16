#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D image;
uniform vec3 spriteColor;

void main()
{
    vec4 texColor = texture(image, TexCoord);
    // Multiply by tint color
    FragColor = vec4(spriteColor, 1.0) * texColor;
    //FragColor = vec4(1.0, 0.0, 0.0, 1.0);
}
