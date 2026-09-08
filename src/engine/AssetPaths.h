#pragma once

#include <string>

namespace engine {

// Resolves a repository-relative path such as "shaders/body.vert" against, in
// order: the current working directory, the directory holding the executable,
// and the source tree the binary was configured from. Reading shaders straight
// out of the source tree means they can be edited and reloaded without a build.
std::string resolveAsset(const std::string& relativePath);

// Reads a whole file; returns false if it cannot be opened.
bool readTextFile(const std::string& path, std::string& out);

}  // namespace engine
