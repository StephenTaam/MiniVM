#include "vm/Opcode.h"

#include <stdexcept>

namespace rhino {

OpcodeRegistry::OpcodeRegistry() {
    const std::vector<std::string> names = {
        "OPCODE_ILL",
        "ALLOC_CLASS_TYPE", "BINARY_OP", "BUILD_DICT", "BUILD_LIST", "CALL", "COMPARE_OP",
        "CREATE_INSTANCE", "DECLARE_IMPORT", "DECLARE_PACKAGE", "DEFINE_CLASS", "DEFINE_FUNCTION",
        "DEFINE_MODULE", "DEFINE_ANNOTATION", "DEFINE_FLAW", "RHINO_ECHO", "FOR_ITER",
        "GARBAGE_COLLECT", "GET_ITER", "JUMP_BACKWARD", "JUMP_FORWARD", "LOAD_ARG", "LOAD_ATTR",
        "LOAD_BUILTIN_FUNC", "LOAD_BUILTIN_FUNC_BY_ID", "LOAD_CONST", "LOAD_CONST_VAR", "LOAD_ENV_VAR",
        "LOAD_FUNCTION", "LOAD_FUNC_VARG", "LOAD_GLOBAL", "LOAD_IMM8", "LOAD_LAMBDA", "LOAD_MODULE",
        "LOAD_NAME", "LOAD_SCRIPT", "LOAD_SENSE_PARSER", "LOAD_SUBSCR", "LOAD_FLAW", "MAKE_FUNCTION",
        "PARSE_MODULE_ATTRS", "POP_JUMP_IF_FALSE", "POP_JUMP_IF_TRUE", "POP_TOP", "RETURN_VALUE",
        "STORE_ATTR", "STORE_CONST_VAR", "STORE_ENV_VAR", "STORE_GLOBAL", "STORE_GLOBAL_CONST",
        "STORE_NAME", "STORE_NAME_CONST", "STORE_SCRIPT", "STORE_SCRIPT_CONST", "STORE_SUBSCR",
        "TO_BOOL", "TO_STRING", "THROW", "CATCH", "REPLACE_FUNCTION", "JUMP_IF_FALSE_OR_POP",
        "JUMP_IF_TRUE_OR_POP", "UNARY_OP", "SLICE", "UNLET_ATTR", "UNLET_GLOBAL", "UNLET_NAME",
        "UNLET_SCRIPT", "UNLET_SUBSCR", "UNPACK_LIST", "UNPACK_DICT", "INSTANCE_OF", "INSTANCE_NOT",
        "DEFER_CALL", "LOAD_NONE", "LOAD_NAMED_PARAM", "BIND_NAMED_PARAM", "LOAD_NOTE", "TYPE_CAST",
        "LOAD_INHERIT", "STORE_INHERIT", "SET_FQN", "UNSET_FQN", "TENNIS_CALL", "EXEC_NETABLOCK",
        "SOURCE_MODULE", "SCRIPT_FINISH", "ADD_MEMORY_CLASS", "GET_CALLER_MODULE", "SLEEP",
        "EXTENDED_ARG", "LOAD_SCRIPT_ID", "LOAD_GADGET_FUNC", "GET_PATH_BY_FQN", "EXECUTE", "SOURCE",
        "GET_FQN_BY_PATH", "UNLET_CONST_VAR", "UNLET_VARIABLE", "LOAD_VARIABLE", "STORE_VARIABLE",
        "DEFER", "STORE_TYPE", "YIELD_VALUE", "RETURN_GENERATOR", "BUILD_GENERICS", "BUILD_RECORD",
        "GENERICS_INSTANCE", "TO_PREDICATE", "LOAD_MICRO_VARIABLE", "LOAD_IMPLICIT_VARIABLE",
        "STORE_VARIABLE_CONST", "LOAD_CLASS_META", "UNLOAD_MODULE", "LOAD_GLOBAL_CONST",
        "LOAD_NAMED_PARAM_CONST", "LOAD_ANNOTATION", "EXIT_SCOPE", "BREAK", "CONTINUE_LOOP",
        "ENTER_LOOP", "LEAVE_LOOP", "CLASS_STATIC_ANNOTATION", "FUNC_STATIC_ANNOTATION",
        "MAKE_MUTABLE", "POP_LOAD", "POP_UNPACK",
        "DEFINE_FUN", "DEFINE_GFUN", "DEFINE_TFUN", "DEFINE_PRELOADER", "DEFINE_CONSTRUCTOR",
        "DEFINE_LAMBDA", "DEFINE_CLOSURE", "DEFINE_ANONYMOUS",
        "LOAD_VAR", "STORE_VAR", "LOAD_GVAR", "STORE_GVAR", "LOAD_TVAR", "STORE_TVAR",
        "LOAD_BVAR", "STORE_BVAR",
        "OPCODE_NUM"
    };

    for (const auto& name : names) {
        add(name);
    }

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
    setKind("RHINO_ECHO", OpargKind::None, 1, 0, "print top of stack");
    setKind("TO_BOOL", OpargKind::None, 1, 1, "convert to bool");
    setKind("JUMP_FORWARD", OpargKind::JumpOffset, 0, 0, "jump forward");
    setKind("JUMP_BACKWARD", OpargKind::JumpOffset, 0, 0, "jump backward");
    setKind("POP_JUMP_IF_FALSE", OpargKind::JumpTarget, 1, 0, "pop and jump if false");
    setKind("POP_JUMP_IF_TRUE", OpargKind::JumpTarget, 1, 0, "pop and jump if true");
    setKind("JUMP_IF_FALSE_OR_POP", OpargKind::JumpTarget, 1, 0, "jump if false, else pop");
    setKind("JUMP_IF_TRUE_OR_POP", OpargKind::JumpTarget, 1, 0, "jump if true, else pop");
}

void OpcodeRegistry::add(const std::string& name, OpargKind kind, int numpop, int numpush, const std::string& description) {
    if (nameToId_.find(name) != nameToId_.end()) {
        return;
    }
    int opcode = static_cast<int>(specs_.size());
    specs_.push_back(OpcodeSpec{name, opcode, numpop, numpush, description, kind});
    nameToId_[name] = opcode;
    idToIndex_[opcode] = specs_.size() - 1;
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
