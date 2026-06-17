#include "runtime/Builtins.h"

#include <iostream>
#include <stdexcept>

namespace rhino {
namespace {

void expectArgCount(const std::vector<Value>& args, size_t expected, const std::string& name) {
    if (args.size() != expected) {
        throw std::runtime_error(name + " expected " + std::to_string(expected) + " args, got " + std::to_string(args.size()));
    }
}

} // namespace

BuiltinMap createBuiltins() {
    BuiltinMap builtins;

    builtins["print"] = Value::builtin("print", [](const std::vector<Value>& args) {
        for (size_t i = 0; i < args.size(); ++i) {
            if (i) {
                std::cout << " ";
            }
            std::cout << args[i].toString();
        }
        std::cout << "\n";
        return Value::null();
    });

    builtins["len"] = Value::builtin("len", [](const std::vector<Value>& args) {
        expectArgCount(args, 1, "len");
        const Value& value = args[0];
        if (value.type == ValueType::String) {
            return Value::integer(static_cast<int64_t>(std::get<std::string>(value.data).size()));
        }
        if (value.type == ValueType::List) {
            return Value::integer(static_cast<int64_t>(std::get<std::shared_ptr<ListValue>>(value.data)->items.size()));
        }
        if (value.type == ValueType::Dict) {
            return Value::integer(static_cast<int64_t>(std::get<std::shared_ptr<DictValue>>(value.data)->items.size()));
        }
        throw std::runtime_error("len unsupported for " + value.repr());
    });

    builtins["str"] = Value::builtin("str", [](const std::vector<Value>& args) {
        expectArgCount(args, 1, "str");
        return Value::string(args[0].toString());
    });

    builtins["int"] = Value::builtin("int", [](const std::vector<Value>& args) {
        expectArgCount(args, 1, "int");
        const Value& value = args[0];
        if (value.type == ValueType::Int) {
            return value;
        }
        if (value.type == ValueType::Double) {
            return Value::integer(static_cast<int64_t>(std::get<double>(value.data)));
        }
        if (value.type == ValueType::Bool) {
            return Value::integer(std::get<bool>(value.data) ? 1 : 0);
        }
        if (value.type == ValueType::String) {
            return Value::integer(std::stoll(std::get<std::string>(value.data)));
        }
        throw std::runtime_error("int unsupported for " + value.repr());
    });

    builtins["bool"] = Value::builtin("bool", [](const std::vector<Value>& args) {
        expectArgCount(args, 1, "bool");
        return Value::boolean(args[0].truthy());
    });

    return builtins;
}

} // namespace rhino
