#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform float threshold;
out vec4 finalColor;
void main() {
    vec3 c = texture(texture0, fragTexCoord).rgb;
    float l = max(max(c.r, c.g), c.b);
    float k = max(l - threshold, 0.0) / max(l, 1e-4);
    finalColor = vec4(c * k, 1.0);
}
