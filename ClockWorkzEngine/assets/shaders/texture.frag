// Lebon
#version 330 core
in vec2 vUV;
in vec4 vColor;
flat in int vTexIndex;

out vec4 FragColor;

uniform sampler2D uTextures[8];
uniform vec4 uTint = vec4(1.0); // Default to white (no tint)

void main() {
    vec4 texColor = texture(uTextures[vTexIndex], vUV);
    FragColor = texColor * vColor * uTint;
}
