#include "engine/AssetPaths.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace engine {
namespace {

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::vector<wchar_t> buffer(4096);
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0) return {};
    return std::filesystem::path(std::wstring(buffer.data(), length)).parent_path();
#else
    return std::filesystem::current_path();
#endif
}

}  // namespace

std::string resolveAsset(const std::string& relativePath) {
    std::vector<std::filesystem::path> roots;
    roots.push_back(std::filesystem::current_path());
    const std::filesystem::path exeDir = executableDirectory();
    if (!exeDir.empty()) {
        roots.push_back(exeDir);
        roots.push_back(exeDir.parent_path());
    }
#ifdef UNIVERSE_SIM_SOURCE_DIR
    roots.emplace_back(UNIVERSE_SIM_SOURCE_DIR);
#endif

    for (const std::filesystem::path& root : roots) {
        std::error_code error;
        const std::filesystem::path candidate = root / relativePath;
        if (std::filesystem::exists(candidate, error)) {
            return candidate.string();
        }
    }
    return relativePath;  // let the caller report the failure with the original name
}

bool readTextFile(const std::string& path, std::string& out) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream stream;
    stream << file.rdbuf();
    out = stream.str();
    return true;
}

}  // namespace engine
