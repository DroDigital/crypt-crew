#version 330
// Lit shader for the KayKit gradient atlas. SKINNED is #defined by the renderer for skinned meshes.
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
#ifdef SKINNED
in vec4 vertexBoneIndices;
in vec4 vertexBoneWeights;
uniform mat4 boneMatrices[64];
#endif
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

out vec3 fragPos;
out vec2 fragTexCoord;
out vec3 fragNormal;

void main() {
    vec4 pos = vec4(vertexPosition, 1.0);
    vec4 nrm = vec4(vertexNormal, 0.0);
#ifdef SKINNED
    mat4 skin = boneMatrices[int(vertexBoneIndices.x)] * vertexBoneWeights.x
              + boneMatrices[int(vertexBoneIndices.y)] * vertexBoneWeights.y
              + boneMatrices[int(vertexBoneIndices.z)] * vertexBoneWeights.z
              + boneMatrices[int(vertexBoneIndices.w)] * vertexBoneWeights.w;
    pos = skin * pos;
    nrm = skin * nrm;
#endif
    fragPos = (matModel * pos).xyz;
    fragTexCoord = vertexTexCoord;
    fragNormal = normalize(mat3(matNormal) * nrm.xyz);
    gl_Position = mvp * pos;
}
