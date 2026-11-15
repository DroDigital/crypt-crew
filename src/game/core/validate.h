#pragma once

namespace game {

// --validate: no window. Parses every glTF under assets/ with cgltf and every data file
// against its expected fields, and cross-checks references (clips, meshes, joints, files).
// Returns the process exit code.
int runValidate(bool verbose);

}  // namespace game
