#include "vm/Opcode.h"

#include <stdexcept>

namespace stt {

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

    setKind("LOAD_CONST_VAR", OpargKind::NameIndex, 0, 1, "load const variable by name");
    setKind("LOAD_GLOBAL_CONST", OpargKind::NameIndex, 0, 1, "load global const variable by name");
    setKind("STORE_CONST_VAR", OpargKind::NameIndex, 1, 0, "store const variable by name");
    setKind("STORE_NAME_CONST", OpargKind::NameIndex, 1, 0, "store const local by name");
    setKind("STORE_GLOBAL_CONST", OpargKind::NameIndex, 1, 0, "store global const variable by name");
    setKind("LOAD_ENV_VAR", OpargKind::NameIndex, 0, 1, "load environment variable by name");
    setKind("STORE_ENV_VAR", OpargKind::NameIndex, 1, 0, "store environment variable by name");
    setKind("LOAD_VARIABLE", OpargKind::NameIndex, 0, 1, "load variable by scoped name");
    setKind("LOAD_MICRO_VARIABLE", OpargKind::NameIndex, 0, 1, "load variable by scoped name");
    setKind("STORE_VARIABLE", OpargKind::NameIndex, 1, 0, "store variable by scoped name");
    setKind("STORE_VARIABLE_CONST", OpargKind::NameIndex, 1, 0, "store const variable by scoped name");
    setKind("UNLET_VARIABLE", OpargKind::NameIndex, 0, 0, "delete variable by scoped name");
    setKind("STORE_TYPE", OpargKind::NameIndex, 1, 0, "store type variable by name");

    setKind("LOAD_FUNCTION", OpargKind::BlockIndex, 0, 1, "load function from blocks[oparg]");
    setKind("REPLACE_FUNCTION", OpargKind::BlockIndex, 0, 1, "replace/load function from blocks[oparg]");
    setKind("LOAD_LAMBDA", OpargKind::BlockIndex, 0, 1, "load lambda from blocks[oparg]");
    setKind("MAKE_FUNCTION", OpargKind::BlockIndex, 0, 1, "make function from blocks[oparg]");
    setKind("LOAD_FUNC_VARG", OpargKind::ImmInt, 0, 1, "load varargs starting at argument index");
    setKind("ALLOC_CLASS_TYPE", OpargKind::BlockIndex, 0, 1, "allocate class type from blocks[oparg]");
    setKind("LOAD_CLASS_META", OpargKind::None, 1, 1, "load class metadata");
    setKind("LOAD_INHERIT", OpargKind::ImmInt, 0, 1, "load inherited class name by index");
    setKind("STORE_INHERIT", OpargKind::NameIndex, 1, 0, "store inherited class/type binding");
    setKind("ADD_MEMORY_CLASS", OpargKind::NameIndex, 1, 0, "store memory class/type binding");

    setKind("LOAD_MODULE", OpargKind::NameIndex, 0, 1, "load module by name");
    setKind("DEFINE_MODULE", OpargKind::NameIndex, -1, 1, "define or load module by name");
    setKind("DECLARE_IMPORT", OpargKind::NameIndex, -1, 0, "declare imported module");
    setKind("SOURCE_MODULE", OpargKind::NameIndex, -1, 0, "source/import module");
    setKind("UNLOAD_MODULE", OpargKind::NameIndex, -1, 0, "unload module by name");
    setKind("DECLARE_PACKAGE", OpargKind::ConstIndex, -1, 0, "declare package name");
    setKind("LOAD_SCRIPT", OpargKind::NameIndex, 0, 1, "load script by name");
    setKind("STORE_SCRIPT", OpargKind::NameIndex, -1, 0, "store script/codeblock by name");
    setKind("STORE_SCRIPT_CONST", OpargKind::NameIndex, -1, 0, "store const script/codeblock by name");
    setKind("UNLET_SCRIPT", OpargKind::NameIndex, 0, 0, "delete script by name");
    setKind("LOAD_SCRIPT_ID", OpargKind::None, 0, 1, "load current script id");
    setKind("LOAD_SENSE_PARSER", OpargKind::NameIndex, 0, 1, "load sense parser metadata");
    setKind("LOAD_GADGET_FUNC", OpargKind::NameIndex, 0, 1, "load gadget/builtin function by name");
    setKind("LOAD_IMPLICIT_VARIABLE", OpargKind::NameIndex, 0, 1, "load implicit variable by name");

    setKind("LOAD_NAMED_PARAM", OpargKind::NameIndex, 0, 1, "load named parameter marker");
    setKind("LOAD_NAMED_PARAM_CONST", OpargKind::NameIndex, 0, 1, "load const named parameter marker");
    setKind("BIND_NAMED_PARAM", OpargKind::NameIndex, 2, 1, "bind top value to named parameter");
    setKind("EXTENDED_ARG", OpargKind::None, -1, 1, "wrap top value as call argument extension");
    setKind("TENNIS_CALL", OpargKind::ArgCount, -1, 1, "call function with tennis-call syntax");
    setKind("DEFER_CALL", OpargKind::ArgCount, -1, 1, "create deferred call metadata");
    setKind("DEFER", OpargKind::ArgCount, -1, 1, "create deferred call metadata");

    setKind("UNLET_ATTR", OpargKind::NameIndex, 1, 0, "delete object attribute");
    setKind("UNLET_GLOBAL", OpargKind::NameIndex, 0, 0, "delete global variable");
    setKind("UNLET_NAME", OpargKind::NameIndex, 0, 0, "delete local variable");
    setKind("UNLET_CONST_VAR", OpargKind::NameIndex, 0, 0, "delete const variable");
    setKind("UNLET_SUBSCR", OpargKind::None, 2, 0, "delete container subscript");
    setKind("UNPACK_LIST", OpargKind::ImmInt, 1, -1, "unpack list/iterator into stack values");
    setKind("POP_UNPACK", OpargKind::ImmInt, 1, -1, "unpack list/iterator into stack values");
    setKind("UNPACK_DICT", OpargKind::ImmInt, 1, -1, "unpack dict into key/value stack values");
    setKind("SLICE", OpargKind::None, 3, 1, "slice list/string");

    setKind("FOR_ITER", OpargKind::JumpTarget, 1, -1, "iterate or jump when iterator is exhausted");
    setKind("BREAK", OpargKind::JumpTarget, 0, 0, "break to jump target");
    setKind("CONTINUE_LOOP", OpargKind::JumpTarget, 0, 0, "continue to jump target");
    setKind("GET_ITER", OpargKind::None, 1, 1, "create iterator");
    setKind("TO_STRING", OpargKind::None, 1, 1, "convert to string");
    setKind("TO_PREDICATE", OpargKind::None, 1, 1, "convert to predicate bool");
    setKind("TYPE_CAST", OpargKind::ConstIndex, 1, 1, "cast value to target type");

    setKind("THROW", OpargKind::NameIndex, -1, 0, "throw flaw/error");
    setKind("CATCH", OpargKind::None, 0, 1, "push pending flaw/error");
    setKind("DEFINE_FLAW", OpargKind::NameIndex, -1, 1, "define flaw/error value");
    setKind("LOAD_FLAW", OpargKind::NameIndex, 0, 1, "load pending or named flaw/error");
    setKind("DEFINE_ANNOTATION", OpargKind::None, -1, 1, "define annotation metadata");
    setKind("LOAD_NOTE", OpargKind::NameIndex, -1, 1, "load note/annotation metadata");
    setKind("LOAD_ANNOTATION", OpargKind::NameIndex, -1, 1, "load annotation metadata");
    setKind("PARSE_MODULE_ATTRS", OpargKind::None, -1, 1, "parse module attribute metadata");
    setKind("EXEC_NETABLOCK", OpargKind::BlockIndex, -1, 1, "execute neta block");
    setKind("BUILD_GENERICS", OpargKind::ImmInt, -1, 1, "build generics metadata");
    setKind("BUILD_RECORD", OpargKind::ImmInt, -1, 1, "build record metadata");
    setKind("GENERICS_INSTANCE", OpargKind::None, 2, 1, "bind generic arguments to value");

    setKind("SET_FQN", OpargKind::NameIndex, -1, 0, "set scoped fully-qualified name");
    setKind("UNSET_FQN", OpargKind::None, 0, 0, "clear scoped fully-qualified name");
    setKind("GET_PATH_BY_FQN", OpargKind::NameIndex, -1, 1, "convert fqn to path");
    setKind("GET_FQN_BY_PATH", OpargKind::None, -1, 1, "convert path to fqn");
    setKind("GET_CALLER_MODULE", OpargKind::None, 0, 1, "load caller module");
    setKind("EXECUTE", OpargKind::None, -1, 1, "create execute-command metadata");
    setKind("SOURCE", OpargKind::NameIndex, -1, 1, "source script or module");
    setKind("SLEEP", OpargKind::ImmInt, 0, 0, "sleep for milliseconds");

    setKind("YIELD_VALUE", OpargKind::None, -1, 1, "yield top value");
    setKind("RETURN_GENERATOR", OpargKind::None, 0, 0, "return generator iterator");
    setKind("SCRIPT_FINISH", OpargKind::None, 0, 0, "finish script frame");
    setKind("INSTANCE_OF", OpargKind::None, 2, 1, "instance-of check");
    setKind("INSTANCE_NOT", OpargKind::None, 2, 1, "negative instance-of check");
    setKind("MAKE_MUTABLE", OpargKind::None, 1, 1, "mark value mutable");
    setKind("CLASS_STATIC_ANNOTATION", OpargKind::None, -1, 0, "store class static annotation");
    setKind("FUNC_STATIC_ANNOTATION", OpargKind::None, -1, 0, "store function static annotation");
    setKind("ENTER_LOOP", OpargKind::None, 0, 0, "loop bookkeeping marker");
    setKind("LEAVE_LOOP", OpargKind::None, 0, 0, "loop bookkeeping marker");
    setKind("EXIT_SCOPE", OpargKind::None, 0, 0, "scope bookkeeping marker");
    setKind("GARBAGE_COLLECT", OpargKind::None, 0, 0, "garbage collection marker");
    setKind("POP_LOAD", OpargKind::NameIndex, 1, 1, "pop top then load name");
    setKind("OPCODE_NUM", OpargKind::None, 0, 1, "push opcode count");
    setKind("OPCODE_ILL", OpargKind::None, 0, 0, "illegal opcode");
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

} // namespace stt
