#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;   // HDR scene
uniform sampler2D bloomTex;
uniform float bloomStrength;
uniform float exposure;
uniform float vignette;
uniform float fade;           // 0 = normal, 1 = black
out vec4 finalColor;

// Linear up to the knee, then a soft shoulder: keeps the KayKit palette intact while
// compressing torch highlights.
vec3 tonemap(vec3 x) {
    const float knee = 0.75;
    vec3 over = max(x - knee, 0.0);
    vec3 soft = knee + (1.0 - knee) * (1.0 - exp(-over / (1.0 - knee)));
    return mix(x, soft, step(knee, x));
}

void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb * exposure;
    c += texture(bloomTex, fragTexCoord).rgb * bloomStrength;
    c = tonemap(c);
    vec2 uv = fragTexCoord - 0.5;
    float v = 1.0 - vignette * smoothstep(0.35, 0.85, length(uv * vec2(1.0, 0.85)) * 1.25);
    c *= v;
    c *= (1.0 - fade);
    finalColor = vec4(clamp(c, 0.0, 1.0), 1.0);
}
