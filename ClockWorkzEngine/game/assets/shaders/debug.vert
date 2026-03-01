// Wei an
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uVP;
out vec3 Color;
void main() {
    gl_Position = uVP * vec4(aPos, 0.0, 1.0);
    Color = aColor;
}