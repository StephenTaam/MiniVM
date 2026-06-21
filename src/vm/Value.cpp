#include "vm/Value.h"

#include "metadata/CodeBlock.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace stt {
namespace {

std::string trimDouble(double value) {
    std::ostringstream oss;
    oss << std::setprecision(15) << value;
    return oss.str();
}

} // namespace

Value::Value() : type(ValueType::Null), data(std::monostate{}) {}

Value Value::null() {
    return Value();
}

Value Value::boolean(bool value) {
    Value out;
    out.type = ValueType::Bool;
    out.data = value;
    return out;
}

Value Value::integer(int64_t value) {
    Value out;
    out.type = ValueType::Int;
    out.data = value;
    return out;
}

Value Value::number(double value) {
    Value out;
    out.type = ValueType::Double;
    out.data = value;
    return out;
}

Value Value::string(std::string value) {
    Value out;
    out.type = ValueType::String;
    out.data = std::move(value);
    return out;
}

Value Value::list(std::vector<Value> values) {
    Value out;
    out.type = ValueType::List;
    auto listValue = std::make_shared<ListValue>();
    listValue->items = std::move(values);
    out.data = listValue;
    return out;
}

Value Value::dict(std::unordered_map<std::string, Value> values) {
    Value out;
    out.type = ValueType::Dict;
    auto dictValue = std::make_shared<DictValue>();
    dictValue->items = std::move(values);
    out.data = dictValue;
    return out;
}

Value Value::function(std::shared_ptr<CodeBlock> codeblock) {
    Value out;
    out.type = ValueType::Function;
    auto fn = std::make_shared<FunctionValue>();
    fn->codeblock = std::move(codeblock);
    out.data = fn;
    return out;
}

Value Value::builtin(std::string name, std::function<Value(const std::vector<Value>&)> fn) {
    Value out;
    out.type = ValueType::BuiltinFunction;
    auto builtin = std::make_shared<BuiltinValue>();
    builtin->name = std::move(name);
    builtin->fn = std::move(fn);
    out.data = builtin;
    return out;
}

Value Value::classType(std::shared_ptr<CodeBlock> codeblock) {
    Value out;
    out.type = ValueType::ClassType;
    auto klass = std::make_shared<ClassValue>();
    klass->codeblock = std::move(codeblock);
    out.data = klass;
    return out;
}

Value Value::instance(std::shared_ptr<ClassValue> classValue) {
    Value out;
    out.type = ValueType::Instance;
    auto instanceValue = std::make_shared<InstanceValue>();
    instanceValue->klass = std::move(classValue);
    out.data = instanceValue;
    return out;
}

Value Value::module(std::string name, std::unordered_map<std::string, Value> attrs) {
    Value out;
    out.type = ValueType::Module;
    auto moduleValue = std::make_shared<ModuleValue>();
    moduleValue->name = std::move(name);
    moduleValue->attrs = std::move(attrs);
    out.data = moduleValue;
    return out;
}

Value Value::iterator(std::vector<Value> items) {
    Value out;
    out.type = ValueType::Iterator;
    auto iteratorValue = std::make_shared<IteratorValue>();
    iteratorValue->items = std::move(items);
    out.data = iteratorValue;
    return out;
}

Value Value::error(std::string name) {
    return error(std::move(name), Value::null());
}

Value Value::error(std::string name, Value payload) {
    Value out;
    out.type = ValueType::Error;
    auto errorValue = std::make_shared<ErrorValue>();
    errorValue->name = std::move(name);
    errorValue->payload = std::move(payload);
    out.data = errorValue;
    return out;
}

bool Value::isNumeric() const {
    return type == ValueType::Int || type == ValueType::Double;
}

double Value::asDouble() const {
    if (type == ValueType::Int) {
        return static_cast<double>(std::get<int64_t>(data));
    }
    if (type == ValueType::Double) {
        return std::get<double>(data);
    }
    throw std::runtime_error("value is not numeric: " + repr());
}

int64_t Value::asInt() const {
    if (type == ValueType::Int) {
        return std::get<int64_t>(data);
    }
    if (type == ValueType::Double) {
        return static_cast<int64_t>(std::get<double>(data));
    }
    throw std::runtime_error("value is not int-compatible: " + repr());
}

bool Value::truthy() const {
    switch (type) {
        case ValueType::Null:
            return false;
        case ValueType::Bool:
            return std::get<bool>(data);
        case ValueType::Int:
            return std::get<int64_t>(data) != 0;
        case ValueType::Double:
            return std::fabs(std::get<double>(data)) > 0.0;
        case ValueType::String:
            return !std::get<std::string>(data).empty();
        case ValueType::List:
            return !std::get<std::shared_ptr<ListValue>>(data)->items.empty();
        case ValueType::Dict:
            return !std::get<std::shared_ptr<DictValue>>(data)->items.empty();
        case ValueType::Iterator: {
            auto iteratorValue = std::get<std::shared_ptr<IteratorValue>>(data);
            return iteratorValue->index < iteratorValue->items.size();
        }
        default:
            return true;
    }
}

std::string Value::toString() const {
    switch (type) {
        case ValueType::Null:
            return "null";
        case ValueType::Bool:
            return std::get<bool>(data) ? "true" : "false";
        case ValueType::Int:
            return std::to_string(std::get<int64_t>(data));
        case ValueType::Double:
            return trimDouble(std::get<double>(data));
        case ValueType::String:
            return std::get<std::string>(data);
        case ValueType::List: {
            std::ostringstream oss;
            const auto& items = std::get<std::shared_ptr<ListValue>>(data)->items;
            oss << "[";
            for (size_t i = 0; i < items.size(); ++i) {
                if (i) {
                    oss << ", ";
                }
                oss << items[i].repr();
            }
            oss << "]";
            return oss.str();
        }
        case ValueType::Dict: {
            std::ostringstream oss;
            const auto& items = std::get<std::shared_ptr<DictValue>>(data)->items;
            oss << "{";
            bool first = true;
            for (const auto& item : items) {
                if (!first) {
                    oss << ", ";
                }
                first = false;
                oss << item.first << ": " << item.second.repr();
            }
            oss << "}";
            return oss.str();
        }
        case ValueType::Function: {
            const auto& fn = std::get<std::shared_ptr<FunctionValue>>(data);
            return "<function " + (fn->codeblock ? fn->codeblock->cbname : std::string("<null>")) + ">";
        }
        case ValueType::BuiltinFunction:
            return "<builtin " + std::get<std::shared_ptr<BuiltinValue>>(data)->name + ">";
        case ValueType::ClassType: {
            const auto& klass = std::get<std::shared_ptr<ClassValue>>(data);
            return "<class " + (klass->codeblock ? klass->codeblock->cbname : std::string("<null>")) + ">";
        }
        case ValueType::Instance: {
            const auto& instanceValue = std::get<std::shared_ptr<InstanceValue>>(data);
            std::string name = instanceValue->klass && instanceValue->klass->codeblock
                ? instanceValue->klass->codeblock->cbname
                : "<null>";
            return "<instance " + name + ">";
        }
        case ValueType::Module:
            return "<module " + std::get<std::shared_ptr<ModuleValue>>(data)->name + ">";
        case ValueType::Iterator: {
            const auto& iteratorValue = std::get<std::shared_ptr<IteratorValue>>(data);
            return "<iterator " + std::to_string(iteratorValue->index) + "/" + std::to_string(iteratorValue->items.size()) + ">";
        }
        case ValueType::Error: {
            const auto& errorValue = std::get<std::shared_ptr<ErrorValue>>(data);
            if (errorValue->payload.type == ValueType::Null) {
                return "<error " + errorValue->name + ">";
            }
            return "<error " + errorValue->name + ": " + errorValue->payload.repr() + ">";
        }
        default:
            return "<" + valueTypeName(type) + ">";
    }
}

std::string Value::repr() const {
    switch (type) {
        case ValueType::Null:
            return "null";
        case ValueType::Bool:
            return std::string("bool(") + (std::get<bool>(data) ? "true" : "false") + ")";
        case ValueType::Int:
            return "int(" + std::to_string(std::get<int64_t>(data)) + ")";
        case ValueType::Double:
            return "double(" + trimDouble(std::get<double>(data)) + ")";
        case ValueType::String:
            return "string(\"" + std::get<std::string>(data) + "\")";
        case ValueType::List:
            return "list(" + toString() + ")";
        case ValueType::Dict:
            return "dict(" + toString() + ")";
        case ValueType::Module:
            return toString();
        case ValueType::Iterator:
            return toString();
        case ValueType::Error:
            return toString();
        default:
            return toString();
    }
}

std::string Value::dictKey() const {
    switch (type) {
        case ValueType::String:
            return std::get<std::string>(data);
        case ValueType::Int:
            return std::to_string(std::get<int64_t>(data));
        case ValueType::Double:
            return trimDouble(std::get<double>(data));
        case ValueType::Bool:
            return std::get<bool>(data) ? "true" : "false";
        case ValueType::Null:
            return "null";
        default:
            return repr();
    }
}

std::string valueTypeName(ValueType type) {
    switch (type) {
        case ValueType::Null: return "Null";
        case ValueType::Bool: return "Bool";
        case ValueType::Int: return "Int";
        case ValueType::Double: return "Double";
        case ValueType::String: return "String";
        case ValueType::List: return "List";
        case ValueType::Dict: return "Dict";
        case ValueType::Function: return "Function";
        case ValueType::BuiltinFunction: return "BuiltinFunction";
        case ValueType::ClassType: return "ClassType";
        case ValueType::Instance: return "Instance";
        case ValueType::Module: return "Module";
        case ValueType::Iterator: return "Iterator";
        case ValueType::Error: return "Error";
    }
    return "Unknown";
}

bool valueEquals(const Value& left, const Value& right) {
    if (left.type == ValueType::Int && right.type == ValueType::Double) {
        return static_cast<double>(std::get<int64_t>(left.data)) == std::get<double>(right.data);
    }
    if (left.type == ValueType::Double && right.type == ValueType::Int) {
        return std::get<double>(left.data) == static_cast<double>(std::get<int64_t>(right.data));
    }
    if (left.type != right.type) {
        return false;
    }
    switch (left.type) {
        case ValueType::Null:
            return true;
        case ValueType::Bool:
            return std::get<bool>(left.data) == std::get<bool>(right.data);
        case ValueType::Int:
            return std::get<int64_t>(left.data) == std::get<int64_t>(right.data);
        case ValueType::Double:
            return std::get<double>(left.data) == std::get<double>(right.data);
        case ValueType::String:
            return std::get<std::string>(left.data) == std::get<std::string>(right.data);
        case ValueType::Function:
            return std::get<std::shared_ptr<FunctionValue>>(left.data) == std::get<std::shared_ptr<FunctionValue>>(right.data);
        case ValueType::BuiltinFunction:
            return std::get<std::shared_ptr<BuiltinValue>>(left.data) == std::get<std::shared_ptr<BuiltinValue>>(right.data);
        case ValueType::ClassType:
            return std::get<std::shared_ptr<ClassValue>>(left.data) == std::get<std::shared_ptr<ClassValue>>(right.data);
        case ValueType::Instance:
            return std::get<std::shared_ptr<InstanceValue>>(left.data) == std::get<std::shared_ptr<InstanceValue>>(right.data);
        case ValueType::Module:
            return std::get<std::shared_ptr<ModuleValue>>(left.data) == std::get<std::shared_ptr<ModuleValue>>(right.data);
        case ValueType::Iterator:
            return std::get<std::shared_ptr<IteratorValue>>(left.data) == std::get<std::shared_ptr<IteratorValue>>(right.data);
        case ValueType::Error:
            return std::get<std::shared_ptr<ErrorValue>>(left.data) == std::get<std::shared_ptr<ErrorValue>>(right.data);
        default:
            return left.toString() == right.toString();
    }
}

int compareValues(const Value& left, const Value& right) {
    if (left.isNumeric() && right.isNumeric()) {
        double a = left.asDouble();
        double b = right.asDouble();
        if (a < b) {
            return -1;
        }
        if (a > b) {
            return 1;
        }
        return 0;
    }
    if (left.type == ValueType::String && right.type == ValueType::String) {
        const auto& a = std::get<std::string>(left.data);
        const auto& b = std::get<std::string>(right.data);
        if (a < b) {
            return -1;
        }
        if (a > b) {
            return 1;
        }
        return 0;
    }
    throw std::runtime_error("cannot compare " + left.repr() + " and " + right.repr());
}

} // namespace stt
