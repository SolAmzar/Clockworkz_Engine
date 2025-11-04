// Lebon
#version 330 core
layout(location = 0) in vec2 aPos;   // light mesh vertex positions only

uniform mat4 uProjection;

out vec2 FragWorldPos;

void main() {
    FragWorldPos = aPos;
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
}
