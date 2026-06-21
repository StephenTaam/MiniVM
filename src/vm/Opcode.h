#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace stt {

enum class OpargKind {
    None,
    ConstIndex,
    NameIndex,
    BlockIndex,
    JumpOffset,
    JumpTarget,
    BinaryOp,
    UnaryOp,
    CompareOp,
    ArgCount,
    ImmInt,
    Unknown
};

struct OpcodeSpec {
    std::string name;
    int opcode = 0;
    int numpop = -1;
    int numpush = -1;
    std::string description;
    OpargKind opargKind = OpargKind::Unknown;
};

class OpcodeRegistry {
public:
    OpcodeRegistry();

    const OpcodeSpec* byName(const std::string& name) const;
    const OpcodeSpec* byId(int opcode) const;
    int idForName(const std::string& name) const;
    std::string nameForId(int opcode) const;
    const std::vector<OpcodeSpec>& specs() const { return specs_; }

private:
    void add(const std::string& name, OpargKind kind = OpargKind::Unknown, int numpop = -1, int numpush = -1, const std::string& description = "");
    void addWithId(const std::string& name, int opcode, OpargKind kind = OpargKind::Unknown, int numpop = -1, int numpush = -1, const std::string& description = "");
    void addAlias(const std::string& aliasName, const std::string& canonicalName);
    void setKind(const std::string& name, OpargKind kind, int numpop = -1, int numpush = -1, const std::string& description = "");

    std::vector<OpcodeSpec> specs_;
    std::unordered_map<std::string, int> nameToId_;
    std::unordered_map<int, size_t> idToIndex_;
};

std::string opargKindName(OpargKind kind);
std::string binaryOpName(int value);
std::string unaryOpName(int value);
std::string compareOpName(int value);

} // namespace stt
