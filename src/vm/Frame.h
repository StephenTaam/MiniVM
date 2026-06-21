#pragma once

#include "vm/Value.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace stt {

struct CodeBlock;

struct Frame {
    std::shared_ptr<CodeBlock> codeblock;
    int ip = 0;
    std::vector<Value> stack;
    std::unordered_map<std::string, Value> locals;
    std::unordered_map<std::string, Value> constVars;
    std::unordered_map<std::string, Value> envVars;
    std::unordered_map<std::string, Value> typeVars;
    std::unordered_map<std::string, Value> scripts;
    std::unordered_map<std::string, Value> tvars;
    std::unordered_map<std::string, Value> bvars;
    std::vector<Value> yields;
    std::vector<Value> args;
    std::string frameName;
    std::string scopedFqn;
};

} // namespace stt
