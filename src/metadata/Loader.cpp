#include "metadata/Loader.h"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace rhino {
namespace {

bool has(const Json& json, const std::string& key) {
    return json.isObject() && json.contains(key);
}

std::string optionalString(const Json& json, const std::string& key, const std::string& fallback = "") {
    return has(json, key) ? json[key].asString() : fallback;
}

int optionalInt(const Json& json, const std::string& key, int fallback = 0) {
    return has(json, key) ? json[key].asInt() : fallback;
}

std::vector<std::string> loadStringArray(const Json& json) {
    std::vector<std::string> out;
    for (const auto& item : json.asArray()) {
        out.push_back(item.asString());
    }
    return out;
}

bool isIntegralJsonNumber(double value) {
    return std::floor(value) == value;
}

} // namespace

std::string codeBlockTypeName(CodeBlockType type) {
    switch (type) {
        case CodeBlockType::Normal: return "normal";
        case CodeBlockType::Function: return "function";
        case CodeBlockType::Class: return "class";
        case CodeBlockType::Neta: return "neta";
    }
    return "normal";
}

CodeBlockType parseCodeBlockType(const std::string& text) {
    if (text == "normal" || text == "Normal") {
        return CodeBlockType::Normal;
    }
    if (text == "function" || text == "Function") {
        return CodeBlockType::Function;
    }
    if (text == "class" || text == "Class") {
        return CodeBlockType::Class;
    }
    if (text == "neta" || text == "Neta") {
        return CodeBlockType::Neta;
    }
    throw std::runtime_error("unknown CodeBlock type: " + text);
}

MetadataLoader::MetadataLoader(const OpcodeRegistry& registry) : registry_(registry) {}

Program MetadataLoader::loadFile(const std::string& path) const {
    return loadJson(Json::parseFile(path));
}

Program MetadataLoader::loadJson(const Json& root) const {
    if (!root.isObject()) {
        throw std::runtime_error("metadata root must be a JSON object");
    }

    Program program;
    program.version = optionalInt(root, "version", 1);
    program.entry = optionalString(root, "entry", "");
    program.root = loadCodeBlock(root["root"]);

    for (const auto& spec : registry_.specs()) {
        program.opcodeNameToId[spec.name] = spec.opcode;
        program.opcodeSpecs[spec.opcode] = spec;
    }

    return program;
}

std::shared_ptr<CodeBlock> MetadataLoader::loadCodeBlock(const Json& json) const {
    if (!json.isObject()) {
        throw std::runtime_error("CodeBlock must be a JSON object");
    }

    auto block = std::make_shared<CodeBlock>();
    block->type = parseCodeBlockType(optionalString(json, "type", "normal"));
    block->cbname = optionalString(json, "cbname", "");
    block->baseline = optionalInt(json, "baseline", 0);
    block->currline = optionalInt(json, "currline", -1);

    if (has(json, "constants")) {
        for (const auto& item : json["constants"].asArray()) {
            block->constants.push_back(loadValue(item));
        }
    }
    if (has(json, "names")) {
        block->names = loadStringArray(json["names"]);
    }
    if (has(json, "blocks")) {
        for (const auto& item : json["blocks"].asArray()) {
            block->blocks.push_back(loadCodeBlock(item));
        }
    }
    if (has(json, "instlnums")) {
        for (const auto& item : json["instlnums"].asArray()) {
            block->instlnums.push_back(item.asInt());
        }
    }
    if (has(json, "instructions")) {
        const auto& instructions = json["instructions"].asArray();
        for (size_t i = 0; i < instructions.size(); ++i) {
            block->instructions.push_back(loadInstruction(instructions[i], static_cast<int>(i), block->instlnums));
        }
    }
    if (has(json, "exceptiontable")) {
        for (const auto& item : json["exceptiontable"].asArray()) {
            block->exceptiontable.push_back(loadExceptionTableItem(item));
        }
    }

    if (has(json, "argnames")) {
        block->argnames = loadStringArray(json["argnames"]);
    }
    if (has(json, "defaults")) {
        for (const auto& item : json["defaults"].asArray()) {
            block->defaults.push_back(loadValue(item));
        }
    }
    if (has(json, "flags")) {
        block->flags = loadStringArray(json["flags"]);
    }
    if (has(json, "inherits")) {
        block->inherits = loadStringArray(json["inherits"]);
    }
    if (has(json, "generics")) {
        block->generics = loadStringArray(json["generics"]);
    }
    if (has(json, "syntactic")) {
        for (const auto& item : json["syntactic"].asObject()) {
            block->syntactic[item.first] = loadValue(item.second);
        }
    }
    if (has(json, "annotations")) {
        for (const auto& item : json["annotations"].asArray()) {
            Annotation annotation;
            annotation.name = optionalString(item, "name", "");
            annotation.value = has(item, "value") ? loadValue(item["value"]) : Value::null();
            block->annotations.push_back(std::move(annotation));
        }
    }

    return block;
}

Value MetadataLoader::loadValue(const Json& json) const {
    if (!json.isObject()) {
        if (json.isNull()) {
            return Value::null();
        }
        if (json.isBool()) {
            return Value::boolean(json.asBool());
        }
        if (json.isNumber()) {
            double number = json.asDouble();
            if (isIntegralJsonNumber(number)) {
                return Value::integer(static_cast<int64_t>(number));
            }
            return Value::number(number);
        }
        if (json.isString()) {
            return Value::string(json.asString());
        }
        if (json.isArray()) {
            std::vector<Value> values;
            for (const auto& item : json.asArray()) {
                values.push_back(loadValue(item));
            }
            return Value::list(std::move(values));
        }
    }

    std::string type = optionalString(json, "type", "");
    if (type.empty()) {
        throw std::runtime_error("typed metadata value is missing 'type'");
    }

    if (type == "null") {
        return Value::null();
    }
    if (type == "bool") {
        return Value::boolean(json["value"].asBool());
    }
    if (type == "int") {
        return Value::integer(static_cast<int64_t>(json["value"].asDouble()));
    }
    if (type == "double" || type == "float" || type == "number") {
        return Value::number(json["value"].asDouble());
    }
    if (type == "string") {
        return Value::string(json["value"].asString());
    }
    if (type == "list") {
        std::vector<Value> values;
        for (const auto& item : json["value"].asArray()) {
            values.push_back(loadValue(item));
        }
        return Value::list(std::move(values));
    }
    if (type == "dict") {
        std::unordered_map<std::string, Value> values;
        for (const auto& item : json["value"].asObject()) {
            values[item.first] = loadValue(item.second);
        }
        return Value::dict(std::move(values));
    }

    throw std::runtime_error("unsupported metadata value type: " + type);
}

Instruction MetadataLoader::loadInstruction(const Json& json, int index, const std::vector<int>& instlnums) const {
    if (!json.isObject()) {
        throw std::runtime_error("instruction must be a JSON object");
    }

    Instruction inst;
    inst.oparg = optionalInt(json, "arg", 0);
    inst.resolvedTarget = optionalInt(json, "target", -1);
    inst.line = optionalInt(json, "line", -1);
    if (inst.line < 0 && index >= 0 && index < static_cast<int>(instlnums.size())) {
        inst.line = instlnums[static_cast<size_t>(index)];
    }

    if (has(json, "op")) {
        inst.opname = json["op"].asString();
        inst.opcode = registry_.idForName(inst.opname);
    } else if (has(json, "opcode")) {
        inst.opcode = json["opcode"].asInt();
        inst.opname = registry_.nameForId(inst.opcode);
    } else {
        throw std::runtime_error("instruction is missing 'op' or 'opcode'");
    }

    return inst;
}

ExceptionTableItem MetadataLoader::loadExceptionTableItem(const Json& json) const {
    ExceptionTableItem item;
    item.startIp = optionalInt(json, "startIp", optionalInt(json, "start", 0));
    item.endIp = optionalInt(json, "endIp", optionalInt(json, "end", 0));
    item.handlerIp = optionalInt(json, "handlerIp", optionalInt(json, "handler", 0));
    item.type = optionalString(json, "type", "");
    return item;
}

} // namespace rhino
