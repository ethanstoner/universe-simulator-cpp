#pragma once

#include <string>
#include <unordered_map>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

namespace render {

// A linked vertex+fragment program. Uniform locations are cached on first use;
// a missing uniform is reported once rather than every frame, because the GLSL
// compiler will happily strip a uniform that the shader does not actually read.
class Shader {
public:
    Shader() = default;
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& other) noexcept;
    Shader& operator=(Shader&& other) noexcept;

    // Paths are repository-relative and go through engine::resolveAsset.
    bool loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    // Recompiles from the same paths; keeps the old program if compilation
    // fails, so a typo while editing a shader does not black out the scene.
    bool reload();

    void bind() const;
    bool valid() const { return program_ != 0; }
    const std::string& lastError() const { return lastError_; }

    void setInt(const char* name, int value);
    void setFloat(const char* name, float value);
    void setVec2(const char* name, const glm::vec2& value);
    void setVec3(const char* name, const glm::vec3& value);
    void setVec4(const char* name, const glm::vec4& value);
    void setMat4(const char* name, const glm::mat4& value);

private:
    int uniformLocation(const char* name);
    static unsigned int compile(unsigned int type, const std::string& source,
                                std::string& error);

    unsigned int program_ = 0;
    std::string vertexPath_;
    std::string fragmentPath_;
    std::string lastError_;
    std::unordered_map<std::string, int> uniformCache_;
};

}  // namespace render
