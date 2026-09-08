#pragma once

#include <string>
#include <vector>

#include "sim/SceneLibrary.h"

namespace sim {

// Scene <-> JSON. Astronomical constants live in configuration rather than
// being scattered through the code: configs/*.json is written from the built-in
// presets and read back at startup, so masses, radii and orbital radii can be
// edited without a rebuild.
//
// Initial conditions round-trip exactly (numbers are written with 17
// significant digits), so a scene loaded from disk reproduces the same
// trajectory as the built-in one it came from.

std::string sceneToJson(const Scene& scene);
bool sceneFromJson(const std::string& text, Scene& out, std::string& error);

bool saveSceneFile(const Scene& scene, const std::string& path, std::string& error);
bool loadSceneFile(const std::string& path, Scene& out, std::string& error);

}  // namespace sim
