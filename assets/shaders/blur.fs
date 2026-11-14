#version 330
in vec2 fragTexCoord;
uniform sampler2D texture0;
uniform vec2 texelDir;   // (1/w, 0) or (0, 1/h)
out vec4 finalColor;
void main() {
    float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 c = texture(texture0, fragTexCoord).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(texture0, fragTexCoord + texelDir * float(i) * 1.5).rgb * w[i];
        c += texture(texture0, fragTexCoord - texelDir * float(i) * 1.5).rgb * w[i];
    }
    finalColor = vec4(c, 1.0);
}
