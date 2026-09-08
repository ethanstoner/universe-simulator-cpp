#include "render/Shader.h"

#include <glad/gl.h>

#include <cstdio>
#include <utility>
#include <vector>

#include "engine/AssetPaths.h"

namespace render {

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

Shader::Shader(Shader&& other) noexcept { *this = std::move(other); }

Shader& Shader::operator=(Shader&& other) noexcept {
    if (this != &other) {
        if (program_) glDeleteProgram(program_);
        program_ = std::exchange(other.program_, 0u);
        vertexPath_ = std::move(other.vertexPath_);
        fragmentPath_ = std::move(other.fragmentPath_);
        lastError_ = std::move(other.lastError_);
        uniformCache_ = std::move(other.uniformCache_);
    }
    return *this;
}

unsigned int Shader::compile(unsigned int type, const std::string& source,
                             std::string& error) {
    const unsigned int shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);

    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        int length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 0 ? length : 1));
        glGetShaderInfoLog(shader, length, nullptr, log.data());
        error = log.data();
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

bool Shader::loadFromFiles(const std::string& vertexPath, const std::string& fragmentPath) {
    vertexPath_ = vertexPath;
    fragmentPath_ = fragmentPath;
    return reload();
}

bool Shader::reload() {
    std::string vertexSource, fragmentSource;
    const std::string vertexFile = engine::resolveAsset(vertexPath_);
    const std::string fragmentFile = engine::resolveAsset(fragmentPath_);

    if (!engine::readTextFile(vertexFile, vertexSource)) {
        lastError_ = "cannot read " + vertexFile;
        std::fprintf(stderr, "[shader] %s\n", lastError_.c_str());
        return false;
    }
    if (!engine::readTextFile(fragmentFile, fragmentSource)) {
        lastError_ = "cannot read " + fragmentFile;
        std::fprintf(stderr, "[shader] %s\n", lastError_.c_str());
        return false;
    }

    std::string error;
    const unsigned int vertex = compile(GL_VERTEX_SHADER, vertexSource, error);
    if (!vertex) {
        lastError_ = vertexPath_ + ": " + error;
        std::fprintf(stderr, "[shader] %s\n", lastError_.c_str());
        return false;
    }
    const unsigned int fragment = compile(GL_FRAGMENT_SHADER, fragmentSource, error);
    if (!fragment) {
        glDeleteShader(vertex);
        lastError_ = fragmentPath_ + ": " + error;
        std::fprintf(stderr, "[shader] %s\n", lastError_.c_str());
        return false;
    }

    const unsigned int program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    int ok = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        int length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        std::vector<char> log(static_cast<std::size_t>(length > 0 ? length : 1));
        glGetProgramInfoLog(program, length, nullptr, log.data());
        lastError_ = std::string("link: ") + log.data();
        std::fprintf(stderr, "[shader] %s\n", lastError_.c_str());
        glDeleteProgram(program);
        return false;  // the previously linked program is left intact
    }

    if (program_) glDeleteProgram(program_);
    program_ = program;
    uniformCache_.clear();
    lastError_.clear();
    return true;
}

void Shader::bind() const {
    if (program_) glUseProgram(program_);
}

int Shader::uniformLocation(const char* name) {
    const auto it = uniformCache_.find(name);
    if (it != uniformCache_.end()) return it->second;
    const int location = glGetUniformLocation(program_, name);
    uniformCache_.emplace(name, location);
    return location;
}

void Shader::setInt(const char* name, int value) {
    const int location = uniformLocation(name);
    if (location >= 0) glUniform1i(location, value);
}
void Shader::setFloat(const char* name, float value) {
    const int location = uniformLocation(name);
    if (location >= 0) glUniform1f(location, value);
}
void Shader::setVec2(const char* name, const glm::vec2& value) {
    const int location = uniformLocation(name);
    if (location >= 0) glUniform2fv(location, 1, &value.x);
}
void Shader::setVec3(const char* name, const glm::vec3& value) {
    const int location = uniformLocation(name);
    if (location >= 0) glUniform3fv(location, 1, &value.x);
}
void Shader::setVec4(const char* name, const glm::vec4& value) {
    const int location = uniformLocation(name);
    if (location >= 0) glUniform4fv(location, 1, &value.x);
}
void Shader::setMat4(const char* name, const glm::mat4& value) {
    const int location = uniformLocation(name);
    if (location >= 0) glUniformMatrix4fv(location, 1, GL_FALSE, &value[0][0]);
}

}  // namespace render
