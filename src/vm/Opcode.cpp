#include "vm/Opcode.h"

#include <stdexcept>

namespace rhino {

OpcodeRegistry::OpcodeRegistry() {
    const std::vector<std::pair<int, std::string>> sourceOpcodes = {
        {0x00, "OPCODE_ILL"},
        {0x01, "ALLOC_CLASS_TYPE"},
        {0x02, "BINARY_OP"},
        {0x03, "BUILD_DICT"},
        {0x04, "BUILD_LIST"},
        {0x05, "CALL"},
        {0x06, "COMPARE_OP"},
        {0x07, "CREATE_INSTANCE"},
        {0x08, "DECLARE_IMPORT"},
        {0x09, "DECLARE_PACKAGE"},
        {0x0a, "DEFINE_CLASS"},
        {0x0c, "DEFINE_MODULE"},
        {0x0d, "DEFINE_ANNOTATION"},
        {0x0e, "DEFINE_FLAW"},
        {0x10, "FOR_ITER"},
        {0x11, "GARBAGE_COLLECT"},
        {0x12, "GET_ITER"},
        {0x13, "JUMP_BACKWARD"},
        {0x14, "JUMP_FORWARD"},
        {0x15, "ECHO"},
        {0x16, "LOAD_ATTR"},
        {0x17, "LOAD_BUILTIN_FUNC"},
        {0x19, "LOAD_CONST"},
        {0x1a, "LOAD_CONST_VAR"},
        {0x1b, "LOAD_ENV_VAR"},
        {0x1c, "LOAD_FUNCTION"},
        {0x1d, "LOAD_FUNC_VARG"},
        {0x1e, "LOAD_GLOBAL"},
        {0x20, "LOAD_LAMBDA"},
        {0x22, "LOAD_NAME"},
        {0x25, "LOAD_SUBSCR"},
        {0x26, "LOAD_FLAW"},
        {0x27, "MAKE_FUNCTION"},
        {0x28, "PARSE_MODULE_ATTRS"},
        {0x29, "POP_JUMP_IF_FALSE"},
        {0x2a, "POP_JUMP_IF_TRUE"},
        {0x2b, "POP_TOP"},
        {0x2c, "RETURN_VALUE"},
        {0x2d, "STORE_ATTR"},
        {0x2e, "STORE_CONST_VAR"},
        {0x2f, "STORE_ENV_VAR"},
        {0x30, "STORE_GLOBAL"},
        {0x32, "STORE_NAME"},
        {0x34, "STORE_SCRIPT"},
        {0x36, "STORE_SUBSCR"},
        {0x37, "TO_BOOL"},
        {0x39, "THROW"},
        {0x3a, "CATCH"},
        {0x3b, "REPLACE_FUNCTION"},
        {0x3c, "JUMP_IF_FALSE_OR_POP"},
        {0x3d, "JUMP_IF_TRUE_OR_POP"},
        {0x3e, "UNARY_OP"},
        {0x3f, "SLICE"},
        {0x40, "UNLET_ATTR"},
        {0x41, "UNLET_GLOBAL"},
        {0x42, "UNLET_NAME"},
        {0x44, "UNLET_SUBSCR"},
        {0x45, "UNPACK_LIST"},
        {0x46, "UNPACK_DICT"},
        {0x47, "INSTANCE_OF"},
        {0x48, "INSTANCE_NOT"},
        {0x49, "DEFER_CALL"},
        {0x4a, "LOAD_NONE"},
        {0x4b, "LOAD_NAMED_PARAM"},
        {0x4c, "BIND_NAMED_PARAM"},
        {0x4d, "LOAD_NOTE"},
        {0x4e, "TYPE_CAST"},
        {0x51, "SET_FQN"},
        {0x52, "UNSET_FQN"},
        {0x53, "TENNIS_CALL"},
        {0x54, "EXEC_NETABLOCK"},
        {0x5a, "EXTENDED_ARG"},
        {0x5d, "GET_PATH_BY_FQN"},
        {0x5e, "EXECUTE"},
        {0x5f, "SOURCE"},
        {0x61, "UNLET_CONST_VAR"},
        {0x62, "UNLET_VARIABLE"},
        {0x63, "LOAD_VARIABLE"},
        {0x64, "STORE_VARIABLE"},
        {0x66, "STORE_TYPE"},
        {0x67, "YIELD_VALUE"},
        {0x68, "RETURN_GENERATOR"},
        {0x69, "BUILD_GENERICS"},
        {0x6a, "BUILD_RECORD"},
        {0x6b, "GENERICS_INSTANCE"},
        {0x6e, "LOAD_IMPLICIT_VARIABLE"}
    };

    for (const auto& item : sourceOpcodes) {
        addWithId(item.second, item.first);
    }

    const std::vector<std::string> trainingOpcodes = {
        "DEFINE_FUNCTION", "LOAD_IMM8", "LOAD_MODULE", "LOAD_SCRIPT", "LOAD_SENSE_PARSER",
        "STORE_GLOBAL_CONST", "STORE_NAME_CONST", "STORE_SCRIPT_CONST", "TO_STRING",
        "UNLET_SCRIPT", "LOAD_INHERIT", "STORE_INHERIT", "SOURCE_MODULE", "SCRIPT_FINISH",
        "ADD_MEMORY_CLASS", "GET_CALLER_MODULE", "SLEEP", "LOAD_SCRIPT_ID", "LOAD_GADGET_FUNC",
        "GET_FQN_BY_PATH", "DEFER", "TO_PREDICATE", "LOAD_MICRO_VARIABLE",
        "STORE_VARIABLE_CONST", "LOAD_CLASS_META", "UNLOAD_MODULE", "LOAD_GLOBAL_CONST",
        "LOAD_NAMED_PARAM_CONST", "LOAD_ANNOTATION", "EXIT_SCOPE", "BREAK", "CONTINUE_LOOP",
        "ENTER_LOOP", "LEAVE_LOOP", "CLASS_STATIC_ANNOTATION", "FUNC_STATIC_ANNOTATION",
        "MAKE_MUTABLE", "POP_LOAD", "POP_UNPACK", "DEFINE_FUN", "DEFINE_GFUN", "DEFINE_TFUN",
        "DEFINE_PRELOADER", "DEFINE_CONSTRUCTOR", "DEFINE_LAMBDA", "DEFINE_CLOSURE",
        "DEFINE_ANONYMOUS", "LOAD_VAR", "STORE_VAR", "LOAD_GVAR", "STORE_GVAR", "LOAD_TVAR",
        "STORE_TVAR", "LOAD_BVAR", "STORE_BVAR", "OPCODE_NUM"
    };

    int syntheticOpcode = 0x100;
    for (const auto& name : trainingOpcodes) {
        addWithId(name, syntheticOpcode++);
    }

    addAlias("RHINO_ECHO", "ECHO");
    addAlias("DEFINE_NOTE", "DEFINE_ANNOTATION");
    addAlias("LOAD_BUILTIN_FUN", "LOAD_BUILTIN_FUNC");
    addAlias("LOAD_FUN_VARG", "LOAD_FUNC_VARG");
    addAlias("LOAD_SPAN", "LOAD_GLOBAL");
    addAlias("STORE_SPAN", "STORE_GLOBAL");
    addAlias("STORE_CODEBLOCK", "STORE_SCRIPT");
    addAlias("CONVERT_BOOL", "TO_BOOL");
    addAlias("UNLET_SPAN", "UNLET_GLOBAL");

    setKind("LOAD_CONST", OpargKind::ConstIndex, 0, 1, "push constants[oparg]");
    setKind("LOAD_IMM8", OpargKind::ImmInt, 0, 1, "push immediate int");
    setKind("LOAD_NONE", OpargKind::None, 0, 1, "push null");
    setKind("LOAD_NAME", OpargKind::NameIndex, 0, 1, "resolve name from locals/globals/builtins");
    setKind("STORE_NAME", OpargKind::NameIndex, 1, 0, "store into locals");
    setKind("LOAD_GLOBAL", OpargKind::NameIndex, 0, 1, "load global/builtin");
    setKind("STORE_GLOBAL", OpargKind::NameIndex, 1, 0, "store global");
    setKind("LOAD_VAR", OpargKind::NameIndex, 0, 1, "load local variable");
    setKind("STORE_VAR", OpargKind::NameIndex, 1, 0, "store local variable");
    setKind("LOAD_GVAR", OpargKind::NameIndex, 0, 1, "load global variable");
    setKind("STORE_GVAR", OpargKind::NameIndex, 1, 0, "store global variable");
    setKind("LOAD_TVAR", OpargKind::NameIndex, 0, 1, "load t-scope variable");
    setKind("STORE_TVAR", OpargKind::NameIndex, 1, 0, "store t-scope variable");
    setKind("LOAD_BVAR", OpargKind::NameIndex, 0, 1, "load b-scope variable");
    setKind("STORE_BVAR", OpargKind::NameIndex, 1, 0, "store b-scope variable");
    setKind("LOAD_ARG", OpargKind::ImmInt, 0, 1, "push argument by index");
    setKind("LOAD_BUILTIN_FUNC", OpargKind::NameIndex, 0, 1, "push builtin function by name");
    setKind("LOAD_ATTR", OpargKind::NameIndex, 1, 1, "load object attribute");
    setKind("STORE_ATTR", OpargKind::NameIndex, 2, 0, "store object attribute");

    setKind("DEFINE_FUN", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_FUNCTION", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_GFUN", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_TFUN", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_PRELOADER", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_CONSTRUCTOR", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_LAMBDA", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_CLOSURE", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_ANONYMOUS", OpargKind::BlockIndex, 0, 1, "create function from blocks[oparg]");
    setKind("DEFINE_CLASS", OpargKind::BlockIndex, 0, 1, "create class from blocks[oparg]");
    setKind("CREATE_INSTANCE", OpargKind::None, 1, 1, "create instance from class");

    setKind("CALL", OpargKind::ArgCount, -1, 1, "call function or builtin");
    setKind("RETURN_VALUE", OpargKind::None, -1, 0, "return top of stack or null");
    setKind("BINARY_OP", OpargKind::BinaryOp, 2, 1, "binary operation");
    setKind("UNARY_OP", OpargKind::UnaryOp, 1, 1, "unary operation");
    setKind("COMPARE_OP", OpargKind::CompareOp, 2, 1, "comparison operation");
    setKind("BUILD_LIST", OpargKind::ImmInt, -1, 1, "build list from N stack values");
    setKind("BUILD_DICT", OpargKind::ImmInt, -1, 1, "build dict from N key/value pairs");
    setKind("LOAD_SUBSCR", OpargKind::None, 2, 1, "container[index]");
    setKind("STORE_SUBSCR", OpargKind::None, 3, 0, "container[index] = value");
    setKind("POP_TOP", OpargKind::None, 1, 0, "discard top of stack");
    setKind("ECHO", OpargKind::None, 1, 0, "print top of stack");
    setKind("TO_BOOL", OpargKind::None, 1, 1, "convert to bool");
    setKind("JUMP_FORWARD", OpargKind::JumpOffset, 0, 0, "jump forward");
    setKind("JUMP_BACKWARD", OpargKind::JumpOffset, 0, 0, "jump backward");
    setKind("POP_JUMP_IF_FALSE", OpargKind::JumpTarget, 1, 0, "pop and jump if false");
    setKind("POP_JUMP_IF_TRUE", OpargKind::JumpTarget, 1, 0, "pop and jump if true");
    setKind("JUMP_IF_FALSE_OR_POP", OpargKind::JumpTarget, 1, 0, "jump if false, else pop");
    setKind("JUMP_IF_TRUE_OR_POP", OpargKind::JumpTarget, 1, 0, "jump if true, else pop");
}

void OpcodeRegistry::add(const std::string& name, OpargKind kind, int numpop, int numpush, const std::string& description) {
    int opcode = specs_.empty() ? 0 : specs_.back().opcode + 1;
    addWithId(name, opcode, kind, numpop, numpush, description);
}

void OpcodeRegistry::addWithId(const std::string& name, int opcode, OpargKind kind, int numpop, int numpush, const std::string& description) {
    if (nameToId_.find(name) != nameToId_.end()) {
        return;
    }
    specs_.push_back(OpcodeSpec{name, opcode, numpop, numpush, description, kind});
    nameToId_[name] = opcode;
    if (idToIndex_.find(opcode) == idToIndex_.end()) {
        idToIndex_[opcode] = specs_.size() - 1;
    }
}

void OpcodeRegistry::addAlias(const std::string& aliasName, const std::string& canonicalName) {
    auto canonical = nameToId_.find(canonicalName);
    if (canonical == nameToId_.end()) {
        return;
    }
    nameToId_[aliasName] = canonical->second;
}

void OpcodeRegistry::setKind(const std::string& name, OpargKind kind, int numpop, int numpush, const std::string& description) {
    auto it = nameToId_.find(name);
    if (it == nameToId_.end()) {
        add(name, kind, numpop, numpush, description);
        return;
    }
    auto& spec = specs_[idToIndex_[it->second]];
    spec.opargKind = kind;
    spec.numpop = numpop;
    spec.numpush = numpush;
    spec.description = description;
}

const OpcodeSpec* OpcodeRegistry::byName(const std::string& name) const {
    auto it = nameToId_.find(name);
    if (it == nameToId_.end()) {
        return nullptr;
    }
    return byId(it->second);
}

const OpcodeSpec* OpcodeRegistry::byId(int opcode) const {
    auto it = idToIndex_.find(opcode);
    if (it == idToIndex_.end()) {
        return nullptr;
    }
    return &specs_[it->second];
}

int OpcodeRegistry::idForName(const std::string& name) const {
    auto it = nameToId_.find(name);
    if (it == nameToId_.end()) {
        throw std::runtime_error("unknown opcode name: " + name);
    }
    return it->second;
}

std::string OpcodeRegistry::nameForId(int opcode) const {
    const OpcodeSpec* spec = byId(opcode);
    return spec ? spec->name : "OPCODE_" + std::to_string(opcode);
}

std::string opargKindName(OpargKind kind) {
    switch (kind) {
        case OpargKind::None: return "None";
        case OpargKind::ConstIndex: return "ConstIndex";
        case OpargKind::NameIndex: return "NameIndex";
        case OpargKind::BlockIndex: return "BlockIndex";
        case OpargKind::JumpOffset: return "JumpOffset";
        case OpargKind::JumpTarget: return "JumpTarget";
        case OpargKind::BinaryOp: return "BinaryOp";
        case OpargKind::UnaryOp: return "UnaryOp";
        case OpargKind::CompareOp: return "CompareOp";
        case OpargKind::ArgCount: return "ArgCount";
        case OpargKind::ImmInt: return "ImmInt";
        case OpargKind::Unknown: return "Unknown";
    }
    return "Unknown";
}

std::string binaryOpName(int value) {
    switch (value) {
        case 0: return "NB_UNKNOWN";
        case 1: return "NB_ADD";
        case 2: return "NB_SUBSTRACT";
        case 3: return "NB_MULTIPLY";
        case 4: return "NB_DEVIDE";
        case 5: return "NB_MOD";
        case 6: return "NB_AND";
        case 7: return "NB_AND2";
        case 8: return "NB_OR";
        case 9: return "NB_OR2";
        case 10: return "NB_XOR";
        case 11: return "NB_XOR2";
        case 12: return "NB_LSHIFT";
        case 13: return "NB_RSHIFT";
        case 14: return "NB_AND3";
        case 15: return "NB_OR3";
        case 16: return "NB_XOR3";
        case 17: return "NB_LSHIFT3";
        case 18: return "NB_RSHIFT3";
        default: return "NB_" + std::to_string(value);
    }
}

std::string unaryOpName(int value) {
    switch (value) {
        case 0: return "UOP_UNKNOWN";
        case 1: return "UOP_PLUS";
        case 2: return "UOP_MINUS";
        case 3: return "UOP_TILDE";
        case 4: return "UOP_NOT";
        case 5: return "UOP_TILDE3";
        default: return "UOP_" + std::to_string(value);
    }
}

std::string compareOpName(int value) {
    switch (value) {
        case 0: return "CMP_UNKNOWN";
        case 1: return "CMP_LT";
        case 2: return "CMP_LE";
        case 3: return "CMP_EQ";
        case 4: return "CMP_NE";
        case 5: return "CMP_GT";
        case 6: return "CMP_GE";
        case 7: return "CMP_PM";
        case 8: return "CMP_RDM";
        case 9: return "CMP_IS";
        case 10: return "CMP_ISNOT";
        default: return "CMP_" + std::to_string(value);
    }
}

} // namespace rhino
