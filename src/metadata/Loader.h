#pragma once

#include "metadata/Program.h"
#include "util/Json.h"
#include "vm/Opcode.h"

#include <memory>
#include <string>

namespace stt {

class MetadataLoader {
public:
    explicit MetadataLoader(const OpcodeRegistry& registry);

    Program loadFile(const std::string& path) const;
    Program loadJson(const Json& root) const;

private:
    std::shared_ptr<CodeBlock> loadCodeBlock(const Json& json) const;
    Value loadValue(const Json& json) const;
    Instruction loadInstruction(const Json& json, int index, const std::vector<int>& instlnums) const;
    ExceptionTableItem loadExceptionTableItem(const Json& json) const;

    const OpcodeRegistry& registry_;
};

} // namespace stt
