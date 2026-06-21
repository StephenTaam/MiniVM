#include "debug/Disassembler.h"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace stt {
namespace {

std::string indent(int depth) {
    return std::string(static_cast<size_t>(depth) * 2, ' ');
}

void dumpStringList(std::ostream& out, const std::string& pad, const std::string& label, const std::vector<std::string>& values) {
    out << pad << label << ":\n";
    if (values.empty()) {
        out << pad << "  empty\n";
        return;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        out << pad << "  [" << i << "] " << values[i] << "\n";
    }
}

void dumpValueList(std::ostream& out, const std::string& pad, const std::string& label, const std::vector<Value>& values) {
    out << pad << label << ":\n";
    if (values.empty()) {
        out << pad << "  empty\n";
        return;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        out << pad << "  [" << i << "] " << values[i].repr() << "\n";
    }
}

void dumpIntList(std::ostream& out, const std::string& pad, const std::string& label, const std::vector<int>& values) {
    out << pad << label << ":\n";
    if (values.empty()) {
        out << pad << "  empty\n";
        return;
    }
    for (size_t i = 0; i < values.size(); ++i) {
        out << pad << "  [" << i << "] " << values[i] << "\n";
    }
}

} // namespace

OpargExplanation explainOparg(const CodeBlock& block, const Instruction& inst, const OpcodeRegistry& registry) {
    OpargExplanation out;
    const OpcodeSpec* spec = registry.byName(inst.opname);
    out.kind = spec ? spec->opargKind : OpargKind::Unknown;

    auto missing = [&](const std::string& table) {
        out.lookup = table + "[" + std::to_string(inst.oparg) + "]";
        out.result = "<out of range>";
        out.summary = out.lookup + " = " + out.result;
    };

    switch (out.kind) {
        case OpargKind::ConstIndex:
            out.lookup = "constants[" + std::to_string(inst.oparg) + "]";
            if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < block.constants.size()) {
                out.result = block.constants[static_cast<size_t>(inst.oparg)].repr();
            } else {
                out.result = "<out of range>";
            }
            out.summary = out.lookup + " = " + out.result;
            break;
        case OpargKind::NameIndex:
            out.lookup = "names[" + std::to_string(inst.oparg) + "]";
            if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < block.names.size()) {
                out.result = block.names[static_cast<size_t>(inst.oparg)];
            } else {
                out.result = "<out of range>";
            }
            out.summary = out.lookup + " = " + out.result;
            break;
        case OpargKind::BlockIndex:
            out.lookup = "blocks[" + std::to_string(inst.oparg) + "]";
            if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < block.blocks.size()) {
                const auto& child = block.blocks[static_cast<size_t>(inst.oparg)];
                out.result = std::to_string(codeBlockTypeId(child->type)) + " " + codeBlockTypeName(child->type) + " " + child->cbname;
            } else {
                out.result = "<out of range>";
            }
            out.summary = out.lookup + " = " + out.result;
            break;
        case OpargKind::BinaryOp:
            out.lookup = "binaryop_t(" + std::to_string(inst.oparg) + ")";
            out.result = binaryOpName(inst.oparg);
            out.summary = out.result;
            break;
        case OpargKind::UnaryOp:
            out.lookup = "unaryop_t(" + std::to_string(inst.oparg) + ")";
            out.result = unaryOpName(inst.oparg);
            out.summary = out.result;
            break;
        case OpargKind::CompareOp:
            out.lookup = "compareop_t(" + std::to_string(inst.oparg) + ")";
            out.result = compareOpName(inst.oparg);
            out.summary = out.result;
            break;
        case OpargKind::ArgCount:
            out.lookup = "arg_count";
            out.result = std::to_string(inst.oparg);
            out.summary = "arg_count = " + out.result;
            break;
        case OpargKind::ImmInt:
            out.lookup = "immediate";
            out.result = std::to_string(inst.oparg);
            out.summary = "imm = " + out.result;
            break;
        case OpargKind::JumpOffset:
        case OpargKind::JumpTarget:
            out.lookup = inst.resolvedTarget >= 0 ? "target" : "arg";
            out.result = inst.resolvedTarget >= 0 ? std::to_string(inst.resolvedTarget) : std::to_string(inst.oparg);
            out.summary = out.lookup + " = " + out.result;
            break;
        case OpargKind::None:
            out.summary = "";
            break;
        case OpargKind::Unknown:
            out.summary = "oparg = " + std::to_string(inst.oparg);
            break;
    }

    if (out.summary.empty() && out.kind != OpargKind::None) {
        missing("?");
    }
    return out;
}

std::string disassembleInstruction(const CodeBlock& block, const Instruction& inst, int ip, const OpcodeRegistry& registry) {
    std::ostringstream oss;
    oss << std::setw(4) << std::setfill('0') << ip << std::setfill(' ') << " "
        << inst.opname << " " << inst.oparg;
    OpargExplanation explanation = explainOparg(block, inst, registry);
    if (!explanation.summary.empty()) {
        oss << " ; " << explanation.summary;
    }
    if (inst.resolvedTarget >= 0) {
        oss << " ; target " << inst.resolvedTarget;
    }
    return oss.str();
}

std::string stackRepr(const std::vector<Value>& stack) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < stack.size(); ++i) {
        if (i) {
            oss << ", ";
        }
        oss << stack[i].repr();
    }
    oss << "]";
    return oss.str();
}

std::string mapRepr(const std::unordered_map<std::string, Value>& values) {
    if (values.empty()) {
        return "{}";
    }
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& item : values) {
        if (!first) {
            oss << ", ";
        }
        first = false;
        oss << item.first << " = " << item.second.repr();
    }
    oss << "}";
    return oss.str();
}

Disassembler::Disassembler(const OpcodeRegistry& registry) : registry_(registry) {}

void Disassembler::dumpProgram(const Program& program, std::ostream& out) const {
    out << "=== Program version=" << program.version;
    if (!program.entry.empty()) {
        out << " entry=" << program.entry;
    }
    out << " ===\n";
    if (program.root) {
        dumpCodeBlock(*program.root, out, 0);
    }
}

void Disassembler::dumpCodeBlock(const CodeBlock& block, std::ostream& out, int depth) const {
    std::string pad = indent(depth);
    out << pad << "=== CodeBlock: " << block.cbname
        << " type=" << codeBlockTypeId(block.type) << " " << codeBlockTypeName(block.type) << " ===\n";
    out << pad << "baseline: " << block.baseline << "\n";
    out << pad << "currline: " << block.currline << "\n";

    dumpValueList(out, pad, "constants", block.constants);
    dumpStringList(out, pad, "names", block.names);

    out << pad << "blocks:\n";
    if (block.blocks.empty()) {
        out << pad << "  empty\n";
    } else {
        for (size_t i = 0; i < block.blocks.size(); ++i) {
            out << pad << "  [" << i << "] type=" << codeBlockTypeId(block.blocks[i]->type)
                << " " << codeBlockTypeName(block.blocks[i]->type) << " " << block.blocks[i]->cbname << "\n";
        }
    }

    dumpStringList(out, pad, "argnames", block.argnames);
    dumpValueList(out, pad, "defaults", block.defaults);
    dumpStringList(out, pad, "flags", block.flags);
    dumpStringList(out, pad, "inherits", block.inherits);
    dumpStringList(out, pad, "generics", block.generics);

    out << pad << "annotations:\n";
    if (block.annotations.empty()) {
        out << pad << "  empty\n";
    } else {
        for (size_t i = 0; i < block.annotations.size(); ++i) {
            out << pad << "  [" << i << "] " << block.annotations[i].name
                << " = " << block.annotations[i].value.repr() << "\n";
        }
    }

    out << pad << "syntactic:\n";
    if (block.syntactic.empty()) {
        out << pad << "  empty\n";
    } else {
        for (const auto& item : block.syntactic) {
            out << pad << "  " << item.first << " = " << item.second.repr() << "\n";
        }
    }

    out << pad << "instructions:\n";
    if (block.instructions.empty()) {
        out << pad << "  empty\n";
    } else {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            out << pad << "  " << disassembleInstruction(block, block.instructions[i], static_cast<int>(i), registry_) << "\n";
        }
    }

    out << pad << "exceptiontable:\n";
    if (block.exceptiontable.empty()) {
        out << pad << "  empty\n";
    } else {
        for (const auto& item : block.exceptiontable) {
            out << pad << "  start=" << item.startIp << " end=" << item.endIp
                << " handler=" << item.handlerIp << " type=" << item.type << "\n";
        }
    }

    dumpIntList(out, pad, "instlnums", block.instlnums);

    for (const auto& child : block.blocks) {
        out << "\n";
        dumpCodeBlock(*child, out, depth + 1);
    }
}

} // namespace stt
