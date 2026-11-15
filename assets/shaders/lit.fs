#version 330
in vec3 fragPos;
in vec2 fragTexCoord;
in vec3 fragNormal;

uniform sampler2D texture0;
uniform vec4 colDiffuse;

uniform vec3 ambientColor;
uniform vec3 sunDir;      // direction the light travels (normalized)
uniform vec3 sunColor;
uniform int lightCount;
uniform vec3 lightPos[8];
uniform vec3 lightColor[8];
uniform float lightRadius[8];
uniform vec3 viewPos;
uniform vec3 fogColor;
uniform vec2 fogRange;    // start, end distance
uniform vec4 tint;        // rgb multiply, a = alpha
uniform float flash;      // 0..1 white hit flash
uniform vec3 emissive;

out vec4 finalColor;

void main() {
    vec4 albedo = texture(texture0, fragTexCoord) * colDiffuse;
    if (albedo.a < 0.1) discard;
    vec3 base = albedo.rgb * tint.rgb;
    vec3 n = normalize(fragNormal);
    if (!gl_FrontFacing) n = -n;

    // Wrapped diffuse keeps the toon-ish KayKit shading soft.
    float sunN = dot(n, -sunDir);
    vec3 light = ambientColor * (0.75 + 0.25 * n.y) + sunColor * max(sunN * 0.7 + 0.3, 0.0);
    for (int i = 0; i < 8; ++i) {
        if (i >= lightCount) break;
        vec3 L = lightPos[i] - fragPos;
        float d = length(L);
        float att = clamp(1.0 - d / lightRadius[i], 0.0, 1.0);
        att *= att;
        float ndl = max(dot(n, L / max(d, 1e-4)) * 0.8 + 0.2, 0.0);
        light += lightColor[i] * att * ndl;
    }
    vec3 c = base * light + emissive;
    c = mix(c, vec3(1.6), clamp(flash, 0.0, 1.0));

    float dist = length(viewPos - fragPos);
    float fog = clamp((dist - fogRange.x) / max(fogRange.y - fogRange.x, 0.001), 0.0, 1.0);
    c = mix(c, fogColor, fog);
    finalColor = vec4(c, albedo.a * tint.a);
}
