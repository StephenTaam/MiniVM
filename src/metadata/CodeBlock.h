#pragma once

#include "vm/Instruction.h"
#include "vm/Value.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rhino {

enum class CodeBlockType {
    Normal,
    Function,
    Class,
    Neta
};

struct Annotation {
    std::string name;
    Value value;
};

struct CodeBlock {
    CodeBlockType type = CodeBlockType::Normal;
    std::string cbname;
    std::vector<Value> constants;
    std::vector<std::string> names;
    std::vector<std::shared_ptr<CodeBlock>> blocks;
    std::vector<Instruction> instructions;
    std::vector<ExceptionTableItem> exceptiontable;
    std::vector<int> instlnums;
    int baseline = 0;
    int currline = -1;

    std::vector<std::string> argnames;
    std::vector<Value> defaults;
    std::vector<std::string> flags;

    std::vector<std::string> inherits;
    std::vector<Annotation> annotations;
    std::unordered_map<std::string, Value> syntactic;
    std::vector<std::string> generics;
};

std::string codeBlockTypeName(CodeBlockType type);
CodeBlockType parseCodeBlockType(const std::string& text);

} // namespace rhino
