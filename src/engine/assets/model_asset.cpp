#include "engine/assets/model_asset.h"

#include <raymath.h>

#include "cgltf.h"

namespace eng {
namespace {

Matrix fromGltf(const float m[16]) {
    // glTF is column-major; raylib Matrix fields are named row-major (m0 m4 m8 m12 = row 0).
    return Matrix{m[0], m[4], m[8],  m[12], m[1], m[5], m[9],  m[13],
                  m[2], m[6], m[10], m[14], m[3], m[7], m[11], m[15]};
}

int jointIndex(const cgltf_skin* skin, const cgltf_node* node) {
    for (cgltf_size i = 0; i < skin->joints_count; ++i)
        if (skin->joints[i] == node) return static_cast<int>(i);
    return -1;
}

bool loadSkeletonFromData(const cgltf_data* d, const std::string& path, ModelAsset& out, std::string& err);

struct CgltfHolder {
    cgltf_data* data = nullptr;
    ~CgltfHolder() {
        if (data) cgltf_free(data);
    }
};

bool parse(const std::string& path, CgltfHolder& h, std::string& err) {
    cgltf_options opt{};
    if (cgltf_parse_file(&opt, path.c_str(), &h.data) != cgltf_result_success) {
        err = "cgltf: cannot parse " + path;
        return false;
    }
    if (cgltf_load_buffers(&opt, h.data, path.c_str()) != cgltf_result_success) {
        err = "cgltf: cannot load buffers for " + path;
        return false;
    }
    if (cgltf_validate(h.data) != cgltf_result_success) {
        err = "cgltf: validation failed for " + path;
        return false;
    }
    return true;
}

}  // namespace

int Skeleton::find(const std::string& name) const {
    for (size_t i = 0; i < jointNames.size(); ++i)
        if (jointNames[i] == name) return static_cast<int>(i);
    return -1;
}

int ModelAsset::findClip(const std::string& name) const {
    for (size_t i = 0; i < clips.size(); ++i)
        if (clips[i].name == name) return static_cast<int>(i);
    return -1;
}

int ModelAsset::findMesh(const std::string& nodeName) const {
    for (size_t i = 0; i < meshNodeNames.size(); ++i)
        if (meshNodeNames[i] == nodeName) return static_cast<int>(i);
    return -1;
}

bool listMeshNodeNames(const std::string& path, std::vector<std::string>& out, std::string& err) {
    CgltfHolder h;
    if (!parse(path, h, err)) return false;
    out.clear();
    for (cgltf_size i = 0; i < h.data->nodes_count; ++i)
        if (h.data->nodes[i].mesh) out.emplace_back(h.data->nodes[i].name ? h.data->nodes[i].name : "");
    return true;
}

bool validateGltf(const std::string& path, std::string& err) {
    CgltfHolder h;
    return parse(path, h, err);
}

bool loadGltfExtras(const std::string& path, ModelAsset& out, std::string& err) {
    CgltfHolder h;
    if (!parse(path, h, err)) return false;
    const cgltf_data* d = h.data;

    // Mesh node names in the same order raylib creates meshes (node order, then primitive order).
    out.meshNodeNames.clear();
    for (cgltf_size i = 0; i < d->nodes_count; ++i) {
        const cgltf_node* n = &d->nodes[i];
        if (!n->mesh) continue;
        for (cgltf_size p = 0; p < n->mesh->primitives_count; ++p)
            if (n->mesh->primitives[p].type == cgltf_primitive_type_triangles)
                out.meshNodeNames.emplace_back(n->name ? n->name : "");
    }
    if (static_cast<int>(out.meshNodeNames.size()) != out.model.meshCount) {
        err = "mesh count mismatch between raylib and cgltf for " + path;
        return false;
    }
    out.meshSkinned.resize(static_cast<size_t>(out.model.meshCount));
    for (int i = 0; i < out.model.meshCount; ++i)
        out.meshSkinned[static_cast<size_t>(i)] = out.model.meshes[i].boneIndices != nullptr;

    return loadSkeletonFromData(d, path, out, err);
}

bool loadSkeletonAndClips(const std::string& path, ModelAsset& out, std::string& err) {
    CgltfHolder h;
    if (!parse(path, h, err)) return false;
    return loadSkeletonFromData(h.data, path, out, err);
}

namespace {
bool loadSkeletonFromData(const cgltf_data* d, const std::string& path, ModelAsset& out, std::string& err) {
    if (d->skins_count == 0) return true;
    const cgltf_skin* skin = &d->skins[0];
    Skeleton& sk = out.skeleton;
    const size_t n = skin->joints_count;
    sk.jointNames.resize(n);
    sk.parent.assign(n, -1);
    sk.rootParent.assign(n, MatrixIdentity());
    sk.inverseBind.assign(n, MatrixIdentity());
    sk.restT.resize(n);
    sk.restR.resize(n);
    sk.restS.resize(n);
    for (size_t j = 0; j < n; ++j) {
        const cgltf_node* node = skin->joints[j];
        sk.jointNames[j] = node->name ? node->name : "";
        int p = node->parent ? jointIndex(skin, node->parent) : -1;
        sk.parent[j] = p;
        if (p < 0 && node->parent) {
            float w[16];
            cgltf_node_transform_world(node->parent, w);
            sk.rootParent[j] = fromGltf(w);
        }
        if (p >= static_cast<int>(j)) {
            err = "joint order is not parent-first in " + path;
            return false;
        }
        float ibm[16];
        if (skin->inverse_bind_matrices && cgltf_accessor_read_float(skin->inverse_bind_matrices, j, ibm, 16)) {
            sk.inverseBind[j] = fromGltf(ibm);
        } else {
            float w[16];
            cgltf_node_transform_world(node, w);
            sk.inverseBind[j] = MatrixInvert(fromGltf(w));
        }
        sk.restT[j] = node->has_translation ? Vector3{node->translation[0], node->translation[1], node->translation[2]}
                                            : Vector3{0, 0, 0};
        sk.restR[j] = node->has_rotation
                          ? Quaternion{node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]}
                          : Quaternion{0, 0, 0, 1};
        sk.restS[j] = node->has_scale ? Vector3{node->scale[0], node->scale[1], node->scale[2]} : Vector3{1, 1, 1};
    }

    out.clips.clear();
    out.clips.reserve(d->animations_count);
    for (cgltf_size a = 0; a < d->animations_count; ++a) {
        const cgltf_animation& an = d->animations[a];
        AnimClip clip;
        clip.name = an.name ? an.name : ("clip" + std::to_string(a));
        for (cgltf_size c = 0; c < an.channels_count; ++c) {
            const cgltf_animation_channel& ch = an.channels[c];
            const int joint = jointIndex(skin, ch.target_node);
            if (joint < 0) continue;
            AnimTrack t;
            t.joint = joint;
            int comps = 3;
            switch (ch.target_path) {
                case cgltf_animation_path_type_translation: t.path = AnimTrack::Path::Translation; break;
                case cgltf_animation_path_type_rotation: t.path = AnimTrack::Path::Rotation; comps = 4; break;
                case cgltf_animation_path_type_scale: t.path = AnimTrack::Path::Scale; break;
                default: continue;
            }
            const cgltf_animation_sampler* s = ch.sampler;
            t.step = s->interpolation == cgltf_interpolation_type_step;
            const bool cubic = s->interpolation == cgltf_interpolation_type_cubic_spline;
            const cgltf_size keys = s->input->count;
            t.times.resize(keys);
            for (cgltf_size k = 0; k < keys; ++k) cgltf_accessor_read_float(s->input, k, &t.times[k], 1);
            t.values.resize(keys * static_cast<size_t>(comps));
            for (cgltf_size k = 0; k < keys; ++k) {
                // Cubic spline stores (in-tangent, value, out-tangent); keep the value only.
                const cgltf_size idx = cubic ? k * 3 + 1 : k;
                cgltf_accessor_read_float(s->output, idx, &t.values[k * static_cast<size_t>(comps)],
                                          static_cast<cgltf_size>(comps));
            }
            if (!t.times.empty()) clip.duration = std::max(clip.duration, t.times.back());
            clip.tracks.push_back(std::move(t));
        }
        out.clips.push_back(std::move(clip));
    }
    return true;
}

}  // namespace

}  // namespace eng
