// Lebon
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;
layout(location = 3) in float aTexIndex;

uniform mat4 uVP;

out vec2 vUV;
out vec4 vColor;
flat out int vTexIndex;

void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    vUV = aUV;
    vColor = aColor;
    vTexIndex = int(aTexIndex);
}