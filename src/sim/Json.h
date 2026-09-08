#pragma once

#include <initializer_list>
#include <map>
#include <string>
#include <vector>

namespace sim {

// A small JSON reader/writer. The project needs exactly one thing from JSON --
// loading and saving scene configurations -- and a single dependency-free file
// is less build surface than pulling in a library for it. Supports the whole
// of RFC 8259 except for the \uXXXX escape, which is passed through verbatim.
class Json {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Json() = default;
    Json(bool value) : type_(Type::Bool), boolean_(value) {}
    Json(double value) : type_(Type::Number), number_(value) {}
    Json(int value) : type_(Type::Number), number_(value) {}
    Json(const char* value) : type_(Type::String), string_(value) {}
    Json(std::string value) : type_(Type::String), string_(std::move(value)) {}

    static Json array() {
        Json json;
        json.type_ = Type::Array;
        return json;
    }
    static Json object() {
        Json json;
        json.type_ = Type::Object;
        return json;
    }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    bool isBool() const { return type_ == Type::Bool; }

    // Typed reads with defaults; a missing or wrong-typed value yields the
    // fallback rather than throwing, because a partially specified config file
    // should still load.
    double asNumber(double fallback = 0.0) const;
    bool asBool(bool fallback = false) const;
    std::string asString(const std::string& fallback = {}) const;

    bool contains(const std::string& key) const;
    const Json& operator[](const std::string& key) const;
    const Json& operator[](std::size_t index) const;
    std::size_t size() const;

    // Builders.
    void set(const std::string& key, Json value);
    void push(Json value);

    std::string dump(int indent = 2) const;

    // Returns false and fills `error` on a malformed document.
    static bool parse(const std::string& text, Json& out, std::string& error);

private:
    void dumpTo(std::string& out, int indent, int depth) const;

    Type type_ = Type::Null;
    bool boolean_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<Json> array_;
    // std::map rather than unordered_map so that a written file has stable,
    // diffable key ordering.
    std::map<std::string, Json> object_;
};

}  // namespace sim
