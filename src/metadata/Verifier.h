#pragma once

#include "metadata/Program.h"
#include "vm/Opcode.h"

#include <string>
#include <vector>

namespace rhino {

class Verifier {
public:
    explicit Verifier(const OpcodeRegistry& registry);

    std::vector<std::string> verify(const Program& program) const;

private:
    void verifyBlock(const CodeBlock& block, const std::string& path, std::vector<std::string>& errors) const;
    void verifyInstruction(const CodeBlock& block, const Instruction& inst, int ip, const std::string& path, std::vector<std::string>& errors) const;

    const OpcodeRegistry& registry_;
};

} // namespace rhino
