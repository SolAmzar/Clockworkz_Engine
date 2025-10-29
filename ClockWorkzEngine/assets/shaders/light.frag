#version 330 core
in vec2 FragWorldPos;
in vec3 Color;
out vec4 FragColor;
uniform vec2 uLightPos;
uniform float uRadius;
void main() {
    float dist = length(FragWorldPos - uLightPos);
    float intensity = clamp(1.0 - dist / uRadius, 0.0, 1.0);
    intensity = pow(intensity, 2.0);
    FragColor = vec4(Color * intensity, intensity);
}
