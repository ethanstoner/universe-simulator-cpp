#include "sim/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace sim {
namespace {

const Json& nullValue() {
    static const Json value;
    return value;
}

struct Parser {
    const std::string& text;
    std::size_t at = 0;
    std::string error;

    explicit Parser(const std::string& source) : text(source) {}

    void skipWhitespace() {
        while (at < text.size()) {
            const char c = text[at];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++at;
            } else if (c == '/' && at + 1 < text.size() && text[at + 1] == '/') {
                // Line comments are not JSON, but a hand-edited config file is
                // far more useful with them and nothing else reads these files.
                while (at < text.size() && text[at] != '\n') ++at;
            } else {
                break;
            }
        }
    }

    bool fail(const std::string& message) {
        if (error.empty()) {
            error = message + " at offset " + std::to_string(at);
        }
        return false;
    }

    bool parseValue(Json& out) {
        skipWhitespace();
        if (at >= text.size()) return fail("unexpected end of input");

        switch (text[at]) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': {
                std::string value;
                if (!parseString(value)) return false;
                out = Json(value);
                return true;
            }
            case 't':
                if (text.compare(at, 4, "true") == 0) {
                    at += 4;
                    out = Json(true);
                    return true;
                }
                return fail("invalid literal");
            case 'f':
                if (text.compare(at, 5, "false") == 0) {
                    at += 5;
                    out = Json(false);
                    return true;
                }
                return fail("invalid literal");
            case 'n':
                if (text.compare(at, 4, "null") == 0) {
                    at += 4;
                    out = Json();
                    return true;
                }
                return fail("invalid literal");
            default: return parseNumber(out);
        }
    }

    bool parseNumber(Json& out) {
        const char* start = text.c_str() + at;
        char* end = nullptr;
        const double value = std::strtod(start, &end);
        if (end == start) return fail("expected a number");
        at += static_cast<std::size_t>(end - start);
        out = Json(value);
        return true;
    }

    bool parseString(std::string& out) {
        if (at >= text.size() || text[at] != '"') return fail("expected a string");
        ++at;
        out.clear();
        while (at < text.size()) {
            const char c = text[at++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (at >= text.size()) return fail("unterminated escape");
            const char escape = text[at++];
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                    // Passed through unchanged: nothing here writes non-ASCII,
                    // and mangling it silently would be worse than keeping it.
                    out.push_back('\\');
                    out.push_back('u');
                    break;
                default: return fail("unknown escape");
            }
        }
        return fail("unterminated string");
    }

    bool parseArray(Json& out) {
        out = Json::array();
        ++at;  // consume '['
        skipWhitespace();
        if (at < text.size() && text[at] == ']') {
            ++at;
            return true;
        }
        while (true) {
            Json element;
            if (!parseValue(element)) return false;
            out.push(std::move(element));
            skipWhitespace();
            if (at >= text.size()) return fail("unterminated array");
            if (text[at] == ',') {
                ++at;
                continue;
            }
            if (text[at] == ']') {
                ++at;
                return true;
            }
            return fail("expected ',' or ']'");
        }
    }

    bool parseObject(Json& out) {
        out = Json::object();
        ++at;  // consume '{'
        skipWhitespace();
        if (at < text.size() && text[at] == '}') {
            ++at;
            return true;
        }
        while (true) {
            skipWhitespace();
            std::string key;
            if (!parseString(key)) return false;
            skipWhitespace();
            if (at >= text.size() || text[at] != ':') return fail("expected ':'");
            ++at;
            Json value;
            if (!parseValue(value)) return false;
            out.set(key, std::move(value));
            skipWhitespace();
            if (at >= text.size()) return fail("unterminated object");
            if (text[at] == ',') {
                ++at;
                continue;
            }
            if (text[at] == '}') {
                ++at;
                return true;
            }
            return fail("expected ',' or '}'");
        }
    }
};

void escapeInto(const std::string& value, std::string& out) {
    out.push_back('"');
    for (char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c);
        }
    }
    out.push_back('"');
}

}  // namespace

double Json::asNumber(double fallback) const {
    return type_ == Type::Number ? number_ : fallback;
}
bool Json::asBool(bool fallback) const {
    return type_ == Type::Bool ? boolean_ : fallback;
}
std::string Json::asString(const std::string& fallback) const {
    return type_ == Type::String ? string_ : fallback;
}

bool Json::contains(const std::string& key) const {
    return type_ == Type::Object && object_.find(key) != object_.end();
}

const Json& Json::operator[](const std::string& key) const {
    if (type_ != Type::Object) return nullValue();
    const auto it = object_.find(key);
    return it == object_.end() ? nullValue() : it->second;
}

const Json& Json::operator[](std::size_t index) const {
    if (type_ != Type::Array || index >= array_.size()) return nullValue();
    return array_[index];
}

std::size_t Json::size() const {
    if (type_ == Type::Array) return array_.size();
    if (type_ == Type::Object) return object_.size();
    return 0;
}

void Json::set(const std::string& key, Json value) {
    if (type_ != Type::Object) {
        type_ = Type::Object;
        object_.clear();
    }
    object_[key] = std::move(value);
}

void Json::push(Json value) {
    if (type_ != Type::Array) {
        type_ = Type::Array;
        array_.clear();
    }
    array_.push_back(std::move(value));
}

void Json::dumpTo(std::string& out, int indent, int depth) const {
    const std::string pad(static_cast<std::size_t>(indent * depth), ' ');
    const std::string padInner(static_cast<std::size_t>(indent * (depth + 1)), ' ');
    const char* newline = indent > 0 ? "\n" : "";

    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += boolean_ ? "true" : "false"; break;
        case Type::Number: {
            char buffer[40];
            // 17 significant digits round-trips an IEEE-754 double exactly,
            // which matters here: these files hold initial conditions and a
            // reload has to reproduce the same trajectory.
            std::snprintf(buffer, sizeof(buffer), "%.17g", number_);
            out += buffer;
            break;
        }
        case Type::String: escapeInto(string_, out); break;
        case Type::Array: {
            if (array_.empty()) {
                out += "[]";
                break;
            }
            out += "[";
            out += newline;
            for (std::size_t i = 0; i < array_.size(); ++i) {
                out += padInner;
                array_[i].dumpTo(out, indent, depth + 1);
                if (i + 1 < array_.size()) out += ",";
                out += newline;
            }
            out += pad;
            out += "]";
            break;
        }
        case Type::Object: {
            if (object_.empty()) {
                out += "{}";
                break;
            }
            out += "{";
            out += newline;
            std::size_t index = 0;
            for (const auto& [key, value] : object_) {
                out += padInner;
                escapeInto(key, out);
                out += indent > 0 ? ": " : ":";
                value.dumpTo(out, indent, depth + 1);
                if (++index < object_.size()) out += ",";
                out += newline;
            }
            out += pad;
            out += "}";
            break;
        }
    }
}

std::string Json::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

bool Json::parse(const std::string& text, Json& out, std::string& error) {
    Parser parser(text);
    if (!parser.parseValue(out)) {
        error = parser.error;
        return false;
    }
    parser.skipWhitespace();
    if (parser.at != text.size()) {
        error = "trailing characters at offset " + std::to_string(parser.at);
        return false;
    }
    error.clear();
    return true;
}

}  // namespace sim
