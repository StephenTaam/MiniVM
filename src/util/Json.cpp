#include "util/Json.h"

#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace rhino {
namespace {

class Parser {
public:
    explicit Parser(const std::string& text) : text_(text) {}

    Json parse() {
        skipWhitespace();
        Json value = parseValue();
        skipWhitespace();
        if (!eof()) {
            fail("unexpected trailing characters");
        }
        return value;
    }

private:
    const std::string& text_;
    size_t pos_ = 0;

    bool eof() const { return pos_ >= text_.size(); }

    char peek() const {
        if (eof()) {
            return '\0';
        }
        return text_[pos_];
    }

    char get() {
        if (eof()) {
            fail("unexpected end of input");
        }
        return text_[pos_++];
    }

    void skipWhitespace() {
        while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
            ++pos_;
        }
    }

    [[noreturn]] void fail(const std::string& message) const {
        std::ostringstream oss;
        oss << "JSON parse error at byte " << pos_ << ": " << message;
        throw JsonError(oss.str());
    }

    Json parseValue() {
        skipWhitespace();
        char c = peek();
        if (c == '"') {
            return Json(parseString());
        }
        if (c == '{') {
            return parseObject();
        }
        if (c == '[') {
            return parseArray();
        }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) {
            return parseNumber();
        }
        if (matchLiteral("true")) {
            return Json(true);
        }
        if (matchLiteral("false")) {
            return Json(false);
        }
        if (matchLiteral("null")) {
            return Json(nullptr);
        }
        fail("expected value");
    }

    bool matchLiteral(const char* literal) {
        size_t start = pos_;
        for (const char* p = literal; *p; ++p) {
            if (eof() || get() != *p) {
                pos_ = start;
                return false;
            }
        }
        return true;
    }

    Json parseObject() {
        Json::Object object;
        expect('{');
        skipWhitespace();
        if (peek() == '}') {
            get();
            return Json(object);
        }

        while (true) {
            skipWhitespace();
            if (peek() != '"') {
                fail("expected object key string");
            }
            std::string key = parseString();
            skipWhitespace();
            expect(':');
            Json value = parseValue();
            object.emplace(std::move(key), std::move(value));
            skipWhitespace();
            char c = get();
            if (c == '}') {
                break;
            }
            if (c != ',') {
                fail("expected ',' or '}'");
            }
        }
        return Json(object);
    }

    Json parseArray() {
        Json::Array array;
        expect('[');
        skipWhitespace();
        if (peek() == ']') {
            get();
            return Json(array);
        }

        while (true) {
            array.push_back(parseValue());
            skipWhitespace();
            char c = get();
            if (c == ']') {
                break;
            }
            if (c != ',') {
                fail("expected ',' or ']'");
            }
        }
        return Json(array);
    }

    Json parseNumber() {
        size_t start = pos_;
        if (peek() == '-') {
            ++pos_;
        }
        if (!std::isdigit(static_cast<unsigned char>(peek()))) {
            fail("expected digit");
        }
        if (peek() == '0') {
            ++pos_;
        } else {
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }
        if (peek() == '.') {
            ++pos_;
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                fail("expected digit after decimal point");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            ++pos_;
            if (peek() == '+' || peek() == '-') {
                ++pos_;
            }
            if (!std::isdigit(static_cast<unsigned char>(peek()))) {
                fail("expected exponent digit");
            }
            while (std::isdigit(static_cast<unsigned char>(peek()))) {
                ++pos_;
            }
        }

        double value = 0.0;
        try {
            value = std::stod(text_.substr(start, pos_ - start));
        } catch (const std::exception&) {
            fail("invalid number");
        }
        return Json(value);
    }

    std::string parseString() {
        expect('"');
        std::string result;
        while (true) {
            if (eof()) {
                fail("unterminated string");
            }
            char c = get();
            if (c == '"') {
                break;
            }
            if (c == '\\') {
                result += parseEscape();
            } else {
                result += c;
            }
        }
        return result;
    }

    std::string parseEscape() {
        char c = get();
        switch (c) {
            case '"': return "\"";
            case '\\': return "\\";
            case '/': return "/";
            case 'b': return "\b";
            case 'f': return "\f";
            case 'n': return "\n";
            case 'r': return "\r";
            case 't': return "\t";
            case 'u': return parseUnicodeEscape();
            default:
                fail("invalid escape sequence");
        }
    }

    std::string parseUnicodeEscape() {
        int code = 0;
        for (int i = 0; i < 4; ++i) {
            if (eof()) {
                fail("unterminated unicode escape");
            }
            char c = get();
            code <<= 4;
            if (c >= '0' && c <= '9') {
                code += c - '0';
            } else if (c >= 'a' && c <= 'f') {
                code += 10 + c - 'a';
            } else if (c >= 'A' && c <= 'F') {
                code += 10 + c - 'A';
            } else {
                fail("invalid unicode hex digit");
            }
        }

        std::string out;
        if (code <= 0x7F) {
            out.push_back(static_cast<char>(code));
        } else if (code <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | ((code >> 6) & 0x1F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | ((code >> 12) & 0x0F)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
        return out;
    }

    void expect(char expected) {
        char actual = get();
        if (actual != expected) {
            std::ostringstream oss;
            oss << "expected '" << expected << "', got '" << actual << "'";
            fail(oss.str());
        }
    }
};

template <typename T>
const T& expectType(Json::Type actual, Json::Type expected, const T& value, const std::string& expectedName) {
    if (actual != expected) {
        throw JsonError("expected JSON " + expectedName);
    }
    return value;
}

} // namespace

Json::Json() = default;
Json::Json(std::nullptr_t) : type_(Type::Null) {}
Json::Json(bool value) : type_(Type::Bool), boolValue_(value) {}
Json::Json(double value) : type_(Type::Number), numberValue_(value) {}
Json::Json(std::string value) : type_(Type::String), stringValue_(std::move(value)) {}
Json::Json(Array value) : type_(Type::Array), arrayValue_(std::move(value)) {}
Json::Json(Object value) : type_(Type::Object), objectValue_(std::move(value)) {}

bool Json::asBool() const {
    if (type_ != Type::Bool) {
        throw JsonError("expected JSON bool");
    }
    return boolValue_;
}

double Json::asDouble() const {
    if (type_ != Type::Number) {
        throw JsonError("expected JSON number");
    }
    return numberValue_;
}

int Json::asInt() const {
    if (type_ != Type::Number) {
        throw JsonError("expected JSON number");
    }
    return static_cast<int>(numberValue_);
}

const std::string& Json::asString() const {
    return expectType(type_, Type::String, stringValue_, "string");
}

const Json::Array& Json::asArray() const {
    return expectType(type_, Type::Array, arrayValue_, "array");
}

const Json::Object& Json::asObject() const {
    return expectType(type_, Type::Object, objectValue_, "object");
}

bool Json::contains(const std::string& key) const {
    if (type_ != Type::Object) {
        return false;
    }
    return objectValue_.find(key) != objectValue_.end();
}

const Json& Json::at(const std::string& key) const {
    if (type_ != Type::Object) {
        throw JsonError("expected JSON object while looking up key '" + key + "'");
    }
    auto it = objectValue_.find(key);
    if (it == objectValue_.end()) {
        throw JsonError("missing JSON key '" + key + "'");
    }
    return it->second;
}

std::string Json::typeName() const {
    switch (type_) {
        case Type::Null: return "null";
        case Type::Bool: return "bool";
        case Type::Number: return "number";
        case Type::String: return "string";
        case Type::Array: return "array";
        case Type::Object: return "object";
    }
    return "unknown";
}

Json Json::parse(const std::string& text) {
    return Parser(text).parse();
}

Json Json::parseFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw JsonError("cannot open JSON file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return parse(buffer.str());
}

} // namespace rhino
