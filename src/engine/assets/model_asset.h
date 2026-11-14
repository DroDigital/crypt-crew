#pragma once
// A loaded glTF model: raylib meshes/materials plus our own skeleton and animation clips
// (sampled with cgltf so we control blending, events and per-clip speed).
#include <raylib.h>

#include <string>
#include <vector>

namespace eng {

struct Skeleton {
    std::vector<std::string> jointNames;
    std::vector<int> parent;          // parent joint index, -1 if parent is not a joint
    std::vector<Matrix> rootParent;   // static world transform of a non-joint parent (identity otherwise)
    std::vector<Matrix> inverseBind;  // per joint
    std::vector<Vector3> restT;
    std::vector<Quaternion> restR;
    std::vector<Vector3> restS;
    int jointCount() const { return static_cast<int>(jointNames.size()); }
    int find(const std::string& name) const;
};

struct AnimTrack {
    enum class Path : unsigned char { Translation, Rotation, Scale };
    int joint = -1;
    Path path = Path::Translation;
    bool step = false;
    std::vector<float> times;
    std::vector<float> values;  // 3 or 4 floats per key
};

struct AnimClip {
    std::string name;
    float duration = 0.f;
    std::vector<AnimTrack> tracks;
};

struct ModelAsset {
    std::string path;
    Model model{};
    std::vector<std::string> meshNodeNames;  // glTF node name per raylib mesh index
    std::vector<bool> meshSkinned;           // mesh has bone data (needs the skinned shader)
    Skeleton skeleton;
    std::vector<AnimClip> clips;
    BoundingBox bounds{};

    bool skinned() const { return skeleton.jointCount() > 0; }
    int findClip(const std::string& name) const;
    int findMesh(const std::string& nodeName) const;
};

// Loads skeleton + clips + mesh node names from a glTF/GLB using cgltf.
// The model itself must already be loaded with raylib's LoadModel.
bool loadGltfExtras(const std::string& path, ModelAsset& out, std::string& err);

// Skeleton + clips only (no GPU, no raylib Model). Used by tools and tests.
bool loadSkeletonAndClips(const std::string& path, ModelAsset& out, std::string& err);

// glTF node names that carry meshes (cgltf only, no GPU).
bool listMeshNodeNames(const std::string& path, std::vector<std::string>& out, std::string& err);

// Parses a glTF/GLB with cgltf only (no GPU). Used by --validate.
bool validateGltf(const std::string& path, std::string& err);

}  // namespace eng
