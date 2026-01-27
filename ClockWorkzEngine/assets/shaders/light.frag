// Lebon
#version 330 core
in vec2 FragWorldPos;
out vec4 FragColor;

uniform vec2 uLightPos;
uniform vec2 uLightDir;     // normalized direction vector (cos,sin)
uniform float uRadius;
uniform float uAngle;       // half-angle of cone in radians
uniform vec3 uLightColor;
uniform float uBattery;   // 0.0 to 1.0 (battery level)
uniform float uFlicker;   // 0.0 to 1.0 (random flicker factor)
uniform float uIntensity; // 0.0 to 1.0 (base opacity/brightness)

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
    
    // Apply base intensity/opacity
    intensity *= uIntensity;

    // Apply battery fading
    intensity *= uBattery;

    // Apply flicker (controlled by C++)
    intensity *= uFlicker; 
    

    //vec3 color = uLightColor * 0.5;
    vec3 color = uLightColor * 0.5;
    //FragColor = vec4(color, intensity);
    FragColor = vec4(uLightColor * intensity, 1.0);
}
