#version 330
// Instanced camera-facing billboards. instanceTransform columns carry:
// [0] = position.xyz, size   [1] = rgba color   [2].x = rotation (radians)
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in mat4 instanceTransform;
uniform mat4 mvp;
uniform vec3 camRight;
uniform vec3 camUp;
out vec2 fragTexCoord;
out vec4 fragColor;
void main() {
    vec4 ps = instanceTransform[0];
    float rot = instanceTransform[2].x;
    float c = cos(rot), s = sin(rot);
    vec2 q = vec2(vertexPosition.x * c - vertexPosition.y * s, vertexPosition.x * s + vertexPosition.y * c) * ps.w;
    vec3 world = ps.xyz + camRight * q.x + camUp * q.y;
    fragTexCoord = vertexTexCoord;
    fragColor = instanceTransform[1];
    gl_Position = mvp * vec4(world, 1.0);
}
