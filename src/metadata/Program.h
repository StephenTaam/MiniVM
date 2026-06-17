#pragma once

#include "metadata/CodeBlock.h"
#include "vm/Opcode.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace rhino {

struct Program {
    int version = 1;
    std::string entry;
    std::shared_ptr<CodeBlock> root;
    std::unordered_map<std::string, int> opcodeNameToId;
    std::unordered_map<int, OpcodeSpec> opcodeSpecs;
};

} // namespace rhino
