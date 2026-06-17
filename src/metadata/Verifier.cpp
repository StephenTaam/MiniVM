#include "metadata/Verifier.h"

#include <sstream>

namespace rhino {
namespace {

bool inRange(int value, size_t size) {
    return value >= 0 && static_cast<size_t>(value) < size;
}

std::string instText(const Instruction& inst) {
    return inst.opname + " " + std::to_string(inst.oparg);
}

} // namespace

Verifier::Verifier(const OpcodeRegistry& registry) : registry_(registry) {}

std::vector<std::string> Verifier::verify(const Program& program) const {
    std::vector<std::string> errors;
    if (!program.root) {
        errors.push_back("program.root is missing");
        return errors;
    }
    verifyBlock(*program.root, program.root->cbname.empty() ? "root" : program.root->cbname, errors);
    return errors;
}

void Verifier::verifyBlock(const CodeBlock& block, const std::string& path, std::vector<std::string>& errors) const {
    if (!block.instlnums.empty() && block.instlnums.size() != block.instructions.size()) {
        errors.push_back(path + ": instlnums size does not match instructions size");
    }

    for (size_t i = 0; i < block.instructions.size(); ++i) {
        verifyInstruction(block, block.instructions[i], static_cast<int>(i), path, errors);
    }

    for (const auto& item : block.exceptiontable) {
        if (!inRange(item.startIp, block.instructions.size()) || !inRange(item.endIp, block.instructions.size())) {
            errors.push_back(path + ": exception table range is outside instruction bounds");
        }
        if (!inRange(item.handlerIp, block.instructions.size())) {
            errors.push_back(path + ": exception handler is outside instruction bounds");
        }
    }

    for (size_t i = 0; i < block.blocks.size(); ++i) {
        if (!block.blocks[i]) {
            errors.push_back(path + ": blocks[" + std::to_string(i) + "] is null");
            continue;
        }
        std::string childName = block.blocks[i]->cbname.empty() ? ("block" + std::to_string(i)) : block.blocks[i]->cbname;
        verifyBlock(*block.blocks[i], path + "/" + childName, errors);
    }
}

void Verifier::verifyInstruction(const CodeBlock& block, const Instruction& inst, int ip, const std::string& path, std::vector<std::string>& errors) const {
    const OpcodeSpec* spec = registry_.byName(inst.opname);
    if (!spec) {
        errors.push_back(path + ":" + std::to_string(ip) + ": unknown opcode " + inst.opname);
        return;
    }

    auto fail = [&](const std::string& what) {
        errors.push_back(path + ":" + std::to_string(ip) + ": " + instText(inst) + " " + what);
    };

    switch (spec->opargKind) {
        case OpargKind::ConstIndex:
            if (!inRange(inst.oparg, block.constants.size())) {
                fail("references missing constants[" + std::to_string(inst.oparg) + "]");
            }
            break;
        case OpargKind::NameIndex:
            if (!inRange(inst.oparg, block.names.size())) {
                fail("references missing names[" + std::to_string(inst.oparg) + "]");
            }
            break;
        case OpargKind::BlockIndex:
            if (!inRange(inst.oparg, block.blocks.size())) {
                fail("references missing blocks[" + std::to_string(inst.oparg) + "]");
            }
            break;
        case OpargKind::JumpTarget:
            if (!inRange(inst.resolvedTarget >= 0 ? inst.resolvedTarget : inst.oparg, block.instructions.size())) {
                fail("has jump target outside instruction bounds");
            }
            break;
        case OpargKind::JumpOffset: {
            int target = inst.resolvedTarget >= 0 ? inst.resolvedTarget : (inst.opname == "JUMP_BACKWARD" ? ip - inst.oparg : ip + inst.oparg);
            if (!inRange(target, block.instructions.size())) {
                fail("has jump offset outside instruction bounds");
            }
            break;
        }
        default:
            break;
    }
}

} // namespace rhino
