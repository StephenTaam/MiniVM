#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace stt {

class JsonError : public std::runtime_error {
public:
    explicit JsonError(const std::string& message) : std::runtime_error(message) {}
};

class Json {
public:
    enum class Type {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;

    Json();
    explicit Json(std::nullptr_t);
    explicit Json(bool value);
    explicit Json(double value);
    explicit Json(std::string value);
    explicit Json(Array value);
    explicit Json(Object value);

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool() const;
    double asDouble() const;
    int asInt() const;
    const std::string& asString() const;
    const Array& asArray() const;
    const Object& asObject() const;

    bool contains(const std::string& key) const;
    const Json& at(const std::string& key) const;
    const Json& operator[](const std::string& key) const { return at(key); }

    std::string typeName() const;

    static Json parse(const std::string& text);
    static Json parseFile(const std::string& path);

private:
    Type type_ = Type::Null;
    bool boolValue_ = false;
    double numberValue_ = 0.0;
    std::string stringValue_;
    Array arrayValue_;
    Object objectValue_;
};

} // namespace stt
