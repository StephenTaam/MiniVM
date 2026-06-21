#pragma once

#include "metadata/Program.h"
#include "vm/Frame.h"
#include "vm/Opcode.h"

#include <iosfwd>
#include <string>

namespace stt {

struct OpargExplanation {
    OpargKind kind = OpargKind::Unknown;
    std::string lookup;
    std::string result;
    std::string summary;
};

OpargExplanation explainOparg(const CodeBlock& block, const Instruction& inst, const OpcodeRegistry& registry);
std::string disassembleInstruction(const CodeBlock& block, const Instruction& inst, int ip, const OpcodeRegistry& registry);
std::string stackRepr(const std::vector<Value>& stack);
std::string mapRepr(const std::unordered_map<std::string, Value>& values);

class Disassembler {
public:
    explicit Disassembler(const OpcodeRegistry& registry);

    void dumpProgram(const Program& program, std::ostream& out) const;
    void dumpCodeBlock(const CodeBlock& block, std::ostream& out, int depth = 0) const;

private:
    const OpcodeRegistry& registry_;
};

} // namespace stt
