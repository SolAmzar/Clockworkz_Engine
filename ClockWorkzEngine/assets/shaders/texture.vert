//#version 330 core
//
//layout(location = 0) in vec2 aPos;
//layout(location = 1) in vec2 aTexCoord;
//
//uniform mat4 uVP; // view * projection (combined)
//uniform mat4 model;
//
//// xy = UV offset (top-left of sprite region in atlas)
//// zw = UV size (width, height) normalized (0–1 range)
//uniform vec4 uvRect;
//
//out vec2 TexCoord;
//
//void main()
//{
//    gl_Position = uVP * model * vec4(aPos, 0.0, 1.0);
//
//    // Remap UVs to the correct region in the sprite sheet
//    TexCoord = uvRect.xy + aTexCoord * uvRect.zw;
//}

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