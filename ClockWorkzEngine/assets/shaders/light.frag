//#version 330 core
//in vec2 FragWorldPos;
//in vec3 Color;
//out vec4 FragColor;
//uniform vec2 uLightPos;
//uniform float uRadius;
//void main() {
//    float dist = length(FragWorldPos - uLightPos);
//    float intensity = clamp(1.0 - dist / uRadius, 0.0, 1.0);
//    intensity = pow(intensity, 2.0);
//    FragColor = vec4(Color * intensity, intensity);
//}

// Version2
//#version 330 core
//in vec2 FragWorldPos;
//out vec4 FragColor;
//
//uniform vec2 uLightPos;
//uniform float uRadius;
//uniform vec3 uLightColor;   // <-- new uniform (e.g. vec3(1.0, 0.9, 0.6))
//
//void main() {
//    float dist = length(FragWorldPos - uLightPos);
//    float intensity = clamp(1.0 - dist / uRadius, 0.0, 1.0);
//    intensity = pow(intensity, 2.0);
//
//    vec3 color = uLightColor * intensity;
//    FragColor = vec4(color, intensity);
//}

#version 330 core
in vec2 FragWorldPos;
out vec4 FragColor;

uniform vec2 uLightPos;
uniform vec2 uLightDir;     // normalized direction vector (cos,sin)
uniform float uRadius;
uniform float uAngle;       // half-angle of cone in radians
uniform vec3 uLightColor;

void main() {
    vec2 toFrag = FragWorldPos - uLightPos;
    float dist = length(toFrag);
    if (dist > uRadius) discard; // early out for performance

    // Directional attenuation (cone)
    vec2 dir = normalize(toFrag);
    float angleCos = dot(dir, normalize(uLightDir));
    float limitCos = cos(uAngle);

    // fade out smoothly near edge of cone
    float coneFactor = smoothstep(limitCos, limitCos + 0.05, angleCos);

    // radial attenuation (distance falloff)
    float radial = 1.0 - dist / uRadius;
    radial = clamp(radial, 0.0, 1.0);
    radial = pow(radial, 2.0);

    float intensity = coneFactor * radial;

    vec3 color = uLightColor * 0.5;
    FragColor = vec4(color, intensity);
}
