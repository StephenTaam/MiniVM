#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace rhino {

struct CodeBlock;
struct ListValue;
struct DictValue;
struct FunctionValue;
struct BuiltinValue;
struct ClassValue;
struct InstanceValue;

enum class ValueType {
    Null,
    Bool,
    Int,
    Double,
    String,
    List,
    Dict,
    Function,
    BuiltinFunction,
    ClassType,
    Instance,
    Module,
    Iterator,
    Error
};

struct Value {
    using Storage = std::variant<
        std::monostate,
        bool,
        int64_t,
        double,
        std::string,
        std::shared_ptr<ListValue>,
        std::shared_ptr<DictValue>,
        std::shared_ptr<FunctionValue>,
        std::shared_ptr<BuiltinValue>,
        std::shared_ptr<ClassValue>,
        std::shared_ptr<InstanceValue>>;

    ValueType type = ValueType::Null;
    Storage data;

    Value();

    static Value null();
    static Value boolean(bool value);
    static Value integer(int64_t value);
    static Value number(double value);
    static Value string(std::string value);
    static Value list(std::vector<Value> values);
    static Value dict(std::unordered_map<std::string, Value> values);
    static Value function(std::shared_ptr<CodeBlock> codeblock);
    static Value builtin(std::string name, std::function<Value(const std::vector<Value>&)> fn);
    static Value classType(std::shared_ptr<CodeBlock> codeblock);
    static Value instance(std::shared_ptr<ClassValue> classValue);

    bool isNumeric() const;
    double asDouble() const;
    int64_t asInt() const;
    bool truthy() const;
    std::string toString() const;
    std::string repr() const;
    std::string dictKey() const;
};

struct ListValue {
    std::vector<Value> items;
};

struct DictValue {
    std::unordered_map<std::string, Value> items;
};

struct FunctionValue {
    std::shared_ptr<CodeBlock> codeblock;
};

struct BuiltinValue {
    std::string name;
    std::function<Value(const std::vector<Value>&)> fn;
};

struct ClassValue {
    std::shared_ptr<CodeBlock> codeblock;
    std::unordered_map<std::string, Value> attrs;
};

struct InstanceValue {
    std::shared_ptr<ClassValue> klass;
    std::unordered_map<std::string, Value> attrs;
};

std::string valueTypeName(ValueType type);
bool valueEquals(const Value& left, const Value& right);
int compareValues(const Value& left, const Value& right);

} // namespace rhino
