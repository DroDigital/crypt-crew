// asset_report: reads every glTF/GLB under assets/ with cgltf and writes a Markdown report
// (node tree, meshes + AABBs, skin joints, animation clips + durations).
// Usage: asset_report <assets_dir> <out.md>
#include "cgltf.h"
#include "engine/anim/animator.h"
#include "engine/assets/model_asset.h"

#include <raymath.h>

#include <algorithm>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Aabb {
    float mn[3] = {FLT_MAX, FLT_MAX, FLT_MAX};
    float mx[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    bool valid() const { return mn[0] <= mx[0]; }
    void add(const float p[3]) {
        for (int i = 0; i < 3; ++i) {
            mn[i] = std::min(mn[i], p[i]);
            mx[i] = std::max(mx[i], p[i]);
        }
    }
};

std::string fmt3(const float v[3]) {
    char buf[96];
    std::snprintf(buf, sizeof buf, "(%.3f, %.3f, %.3f)", v[0], v[1], v[2]);
    return buf;
}

std::string nodeName(const cgltf_node* n) { return n->name ? n->name : "<unnamed>"; }

bool isJoint(const cgltf_data* d, const cgltf_node* n) {
    for (cgltf_size s = 0; s < d->skins_count; ++s)
        for (cgltf_size j = 0; j < d->skins[s].joints_count; ++j)
            if (d->skins[s].joints[j] == n) return true;
    return false;
}

void writeTree(std::ostream& o, const cgltf_data* d, const cgltf_node* n, int depth) {
    o << std::string(static_cast<size_t>(depth) * 2, ' ') << "- " << nodeName(n);
    if (n->mesh) o << "  [mesh: " << (n->mesh->name ? n->mesh->name : "?") << (n->skin ? ", skinned" : "") << "]";
    if (isJoint(d, n)) o << "  [joint]";
    o << "\n";
    for (cgltf_size c = 0; c < n->children_count; ++c) writeTree(o, d, n->children[c], depth + 1);
}

// World-space AABB of a node's mesh from accessor min/max (8 corners transformed).
Aabb meshAabb(const cgltf_node* n) {
    Aabb box;
    float world[16];
    cgltf_node_transform_world(n, world);
    for (cgltf_size p = 0; p < n->mesh->primitives_count; ++p) {
        const cgltf_primitive& prim = n->mesh->primitives[p];
        for (cgltf_size a = 0; a < prim.attributes_count; ++a) {
            const cgltf_accessor* acc = prim.attributes[a].data;
            if (prim.attributes[a].type != cgltf_attribute_type_position || !acc->has_min || !acc->has_max) continue;
            for (int c = 0; c < 8; ++c) {
                float l[3] = {(c & 1) ? acc->max[0] : acc->min[0], (c & 2) ? acc->max[1] : acc->min[1],
                              (c & 4) ? acc->max[2] : acc->min[2]};
                // Skinned meshes are authored in bind space; node transform is ignored per glTF spec.
                float w[3];
                if (n->skin) {
                    w[0] = l[0]; w[1] = l[1]; w[2] = l[2];
                } else {
                    for (int r = 0; r < 3; ++r) w[r] = world[r] * l[0] + world[4 + r] * l[1] + world[8 + r] * l[2] + world[12 + r];
                }
                box.add(w);
            }
        }
    }
    return box;
}

float clipDuration(const cgltf_animation& an) {
    float dur = 0.f;
    for (cgltf_size s = 0; s < an.samplers_count; ++s) {
        const cgltf_accessor* in = an.samplers[s].input;
        if (in->has_max) dur = std::max(dur, in->max[0]);
        else if (in->count > 0) {
            float t = 0.f;
            cgltf_accessor_read_float(in, in->count - 1, &t, 1);
            dur = std::max(dur, t);
        }
    }
    return dur;
}

// For attack-like clips: the time at which the right (or left) hand slot moves fastest.
// This is the data-derived "hit frame" used in data/anims/*.json.
struct HandPeak {
    float time = -1.f;
    float speed = 0.f;
};

HandPeak handPeak(eng::ModelAsset& asset, int clip, const char* jointName) {
    HandPeak best;
    const int joint = asset.skeleton.find(jointName);
    if (joint < 0) return best;
    eng::Animator an;
    an.init(&asset);
    an.play(clip, eng::PlayParams{1.f, false, 0.f, true});
    const float dt = 1.f / 120.f;
    an.evaluate();
    Vector3 prev = Vector3Transform(Vector3{0, 0, 0}, an.jointModel(joint));
    const float dur = asset.clips[static_cast<size_t>(clip)].duration;
    for (float t = dt; t <= dur; t += dt) {
        an.update(dt);
        an.evaluate();
        Vector3 p = Vector3Transform(Vector3{0, 0, 0}, an.jointModel(joint));
        const float v = Vector3Distance(p, prev) / dt;
        if (v > best.speed) {
            best.speed = v;
            best.time = t;
        }
        prev = p;
    }
    return best;
}

bool isActionClip(const std::string& n) {
    for (const char* k : {"Attack", "Spellcast", "Throw", "Shoot", "Block", "Punch", "Kick"})
        if (n.find(k) != std::string::npos) return true;
    return false;
}

bool reportFile(std::ostream& o, const fs::path& file, const fs::path& root, bool brief) {
    cgltf_options opt{};
    cgltf_data* d = nullptr;
    const std::string path = file.string();
    if (cgltf_parse_file(&opt, path.c_str(), &d) != cgltf_result_success) {
        o << "### " << fs::relative(file, root).generic_string() << "\n\n**FAILED TO PARSE**\n\n";
        return false;
    }
    if (cgltf_load_buffers(&opt, d, path.c_str()) != cgltf_result_success) {
        o << "### " << fs::relative(file, root).generic_string() << "\n\n**FAILED TO LOAD BUFFERS**\n\n";
        cgltf_free(d);
        return false;
    }
    o << "### " << fs::relative(file, root).generic_string() << "\n\n";
    o << "nodes: " << d->nodes_count << ", meshes: " << d->meshes_count << ", skins: " << d->skins_count
      << ", animations: " << d->animations_count << ", images: " << d->images_count << "\n\n";

    Aabb total;
    std::ostringstream meshes;
    for (cgltf_size i = 0; i < d->nodes_count; ++i) {
        const cgltf_node* n = &d->nodes[i];
        if (!n->mesh) continue;
        Aabb b = meshAabb(n);
        if (!b.valid()) continue;
        total.add(b.mn);
        total.add(b.mx);
        meshes << "| " << nodeName(n) << " | " << (n->mesh->name ? n->mesh->name : "?") << " | "
               << (n->parent ? nodeName(n->parent) : "-") << " | " << (n->skin ? "yes" : "no") << " | " << fmt3(b.mn)
               << " | " << fmt3(b.mx) << " |\n";
    }
    if (total.valid()) {
        float size[3] = {total.mx[0] - total.mn[0], total.mx[1] - total.mn[1], total.mx[2] - total.mn[2]};
        o << "Overall AABB: min " << fmt3(total.mn) << " max " << fmt3(total.mx) << " size " << fmt3(size) << "\n\n";
    }
    if (!brief) {
        o << "<details><summary>Node tree</summary>\n\n";
        for (cgltf_size s = 0; s < d->scenes_count; ++s)
            for (cgltf_size r = 0; r < d->scenes[s].nodes_count; ++r) writeTree(o, d, d->scenes[s].nodes[r], 0);
        o << "\n</details>\n\n";
    }
    o << "| Node | Mesh | Parent | Skinned | AABB min | AABB max |\n|---|---|---|---|---|---|\n" << meshes.str() << "\n";

    for (cgltf_size s = 0; s < d->skins_count; ++s) {
        o << "Skin " << s << " joints (" << d->skins[s].joints_count << "): ";
        for (cgltf_size j = 0; j < d->skins[s].joints_count; ++j)
            o << (j ? ", " : "") << "`" << nodeName(d->skins[s].joints[j]) << "`";
        o << "\n\n";
    }
    if (d->animations_count > 0) {
        eng::ModelAsset asset;
        std::string err;
        const bool sampled = eng::loadSkeletonAndClips(path, asset, err) && asset.clips.size() == d->animations_count;
        o << "| # | Clip | Duration (s) | Channels | Peak hand.r speed at (s) | Peak hand.l speed at (s) |\n"
             "|---|---|---|---|---|---|\n";
        for (cgltf_size a = 0; a < d->animations_count; ++a) {
            const std::string name = d->animations[a].name ? d->animations[a].name : "?";
            char dur[32];
            std::snprintf(dur, sizeof dur, "%.3f", clipDuration(d->animations[a]));
            std::string pr = "-", pl = "-";
            if (sampled && isActionClip(name)) {
                char b[32];
                HandPeak r = handPeak(asset, static_cast<int>(a), "handslot.r");
                HandPeak l = handPeak(asset, static_cast<int>(a), "handslot.l");
                std::snprintf(b, sizeof b, "%.3f", r.time);
                pr = b;
                std::snprintf(b, sizeof b, "%.3f", l.time);
                pl = b;
            }
            o << "| " << a << " | " << name << " | " << dur << " | " << d->animations[a].channels_count << " | " << pr
              << " | " << pl << " |\n";
        }
        o << "\n";
    }
    cgltf_free(d);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: asset_report <assets_dir> <out.md>\n");
        return 2;
    }
    const fs::path root = argv[1];
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator(root)) {
        if (!e.is_regular_file()) continue;
        const auto ext = e.path().extension().string();
        if (ext == ".glb" || ext == ".gltf") files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());

    std::ofstream out(argv[2]);
    out << "# Asset report\n\nGenerated by `tools/asset_report` (cgltf). Do not edit by hand; re-run:\n\n"
        << "```\nbuild/release/asset_report assets docs/asset_report.md\n```\n\n";
    out << "## Summary\n\n"
        << "- Animations live **inside each character GLB** (heroes: Knight/Barbarian/Mage/Rogue, skeletons: "
           "Minion/Warrior/Rogue/Mage). There are no separate animation files; each character carries its own copy of the clips.\n"
        << "- Weapon/prop attachments are listed as non-skinned meshes parented to joints (see the Parent column).\n"
        << "- Dungeon pieces each embed the same `dungeon_texture` atlas; the game shares one GPU copy.\n\n";

    int failures = 0;
    for (const auto& f : files) {
        const auto rel = fs::relative(f, root).generic_string();
        const bool brief = rel.rfind("kaykit_dungeon", 0) == 0;  // keep the 200 dungeon pieces compact
        if (!reportFile(out, f, root, brief)) ++failures;
    }
    std::printf("asset_report: %zu files, %d failures\n", files.size(), failures);
    return failures == 0 ? 0 : 1;
}
