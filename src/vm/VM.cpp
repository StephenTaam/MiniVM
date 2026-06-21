#include "vm/VM.h"

#include "debug/Disassembler.h"

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace stt {
namespace {

bool isFunctionDefineOpcode(const std::string& opname) {
    static const std::unordered_set<std::string> names = {
        "DEFINE_FUN", "DEFINE_FUNCTION", "DEFINE_GFUN", "DEFINE_TFUN", "DEFINE_PRELOADER",
        "DEFINE_CONSTRUCTOR", "DEFINE_LAMBDA", "DEFINE_CLOSURE", "DEFINE_ANONYMOUS"
    };
    return names.find(opname) != names.end();
}

bool shouldReturnInt(const Value& left, const Value& right) {
    return left.type == ValueType::Int && right.type == ValueType::Int;
}

void requireInts(const Value& left, const Value& right, const std::string& opname) {
    if (left.type != ValueType::Int || right.type != ValueType::Int) {
        throw std::runtime_error("unsupported operands for " + opname + ": " + left.repr() + ", " + right.repr());
    }
}

bool patternMatch(const Value& value, const Value& pattern) {
    std::string text = value.toString();
    std::string pat = pattern.toString();
    try {
        return std::regex_search(text, std::regex(pat));
    } catch (const std::regex_error&) {
        return text.find(pat) != std::string::npos;
    }
}

struct ScopedName {
    std::string scope;
    std::string name;
};

ScopedName splitScopedName(const std::string& raw) {
    size_t pos = raw.find(':');
    if (pos == std::string::npos) {
        return {"", raw};
    }
    return {raw.substr(0, pos), raw.substr(pos + 1)};
}

bool scopeMatches(const std::string& scope, std::initializer_list<const char*> names) {
    for (const char* name : names) {
        if (scope == name) {
            return true;
        }
    }
    return false;
}

int64_t normalizeIndex(int64_t index, size_t size) {
    if (index < 0) {
        index += static_cast<int64_t>(size);
    }
    return index;
}

int64_t normalizeSliceIndex(int64_t index, size_t size) {
    if (index < 0) {
        index += static_cast<int64_t>(size);
    }
    if (index < 0) {
        return 0;
    }
    int64_t max = static_cast<int64_t>(size);
    if (index > max) {
        return max;
    }
    return index;
}

bool isCastTargetName(const std::string& target) {
    return target == "1" || target == "int" || target == "Int" ||
        target == "2" || target == "double" || target == "float" || target == "number" ||
        target == "3" || target == "string" || target == "String" ||
        target == "4" || target == "bool" || target == "Bool" || target == "boolean";
}

} // namespace

VM::VM(Program program, const OpcodeRegistry& registry)
    : program_(std::move(program)), registry_(registry), pendingError_(Value::null()), builtins_(createBuiltins()) {}

Value VM::run(const std::string& entryOverride) {
    if (!program_.root) {
        throw RuntimeError("program root is missing");
    }

    std::shared_ptr<CodeBlock> entry = program_.root;
    if (!entryOverride.empty()) {
        entry = findCodeBlock(program_.root, entryOverride);
        if (!entry) {
            throw RuntimeError("entry CodeBlock not found: " + entryOverride);
        }
    } else if (program_.root->instructions.empty() && !program_.entry.empty()) {
        auto found = findCodeBlock(program_.root, program_.entry);
        if (found) {
            entry = found;
        }
    }

    return executeBlock(entry, {});
}

Value VM::executeBlock(
    std::shared_ptr<CodeBlock> codeblock,
    const std::vector<Value>& args,
    const std::unordered_map<std::string, Value>& namedArgs) {
    Frame frame;
    frame.codeblock = std::move(codeblock);
    frame.ip = 0;
    frame.args = args;
    frame.frameName = frame.codeblock && !frame.codeblock->cbname.empty() ? frame.codeblock->cbname : "<anonymous>";
    bindArgs(frame, args, namedArgs);
    frames_.push_back(std::move(frame));

    while (currentFrame().ip >= 0 && currentFrame().ip < static_cast<int>(currentFrame().codeblock->instructions.size())) {
        Frame& frameRef = currentFrame();
        int ip = frameRef.ip;
        const Instruction& inst = frameRef.codeblock->instructions[static_cast<size_t>(ip)];
        std::vector<Value> beforeStack = frameRef.stack;
        activeInst_ = &inst;
        activeIp_ = ip;
        activeStackBefore_ = beforeStack;

        int nextIp = ip + 1;
        bool didReturn = false;
        Value returnValue = Value::null();
        try {
            executeInstruction(inst, ip, nextIp, didReturn, returnValue);
        } catch (const RuntimeError&) {
            throw;
        } catch (const std::exception& ex) {
            runtimeError(ex.what());
        }

        if (trace) {
            traceInstruction(inst, ip, beforeStack);
        }

        activeInst_ = nullptr;
        activeIp_ = -1;
        activeStackBefore_.clear();

        if (didReturn) {
            frames_.pop_back();
            return returnValue;
        }

        currentFrame().ip = nextIp;
    }

    frames_.pop_back();
    return Value::null();
}

void VM::bindArgs(
    Frame& frame,
    const std::vector<Value>& args,
    const std::unordered_map<std::string, Value>& namedArgs) {
    if (!frame.codeblock) {
        return;
    }

    const auto& argnames = frame.codeblock->argnames;
    if (!argnames.empty() && args.size() > argnames.size()) {
        throw RuntimeError("too many arguments for " + frame.frameName);
    }

    size_t defaultsStart = argnames.size() >= frame.codeblock->defaults.size()
        ? argnames.size() - frame.codeblock->defaults.size()
        : 0;
    std::vector<Value> boundArgs;
    boundArgs.reserve(argnames.size());
    for (size_t i = 0; i < argnames.size(); ++i) {
        auto named = namedArgs.find(argnames[i]);
        if (i < args.size()) {
            if (named != namedArgs.end()) {
                throw RuntimeError("argument passed twice: " + argnames[i]);
            }
            frame.locals[argnames[i]] = args[i];
            boundArgs.push_back(args[i]);
        } else if (named != namedArgs.end()) {
            frame.locals[argnames[i]] = named->second;
            boundArgs.push_back(named->second);
        } else if (i >= defaultsStart && (i - defaultsStart) < frame.codeblock->defaults.size()) {
            frame.locals[argnames[i]] = frame.codeblock->defaults[i - defaultsStart];
            boundArgs.push_back(frame.codeblock->defaults[i - defaultsStart]);
        } else {
            frame.locals[argnames[i]] = Value::null();
            boundArgs.push_back(Value::null());
        }
    }
    for (const auto& item : namedArgs) {
        bool declared = std::find(argnames.begin(), argnames.end(), item.first) != argnames.end();
        if (!declared) {
            frame.locals[item.first] = item.second;
        }
    }
    if (!boundArgs.empty()) {
        frame.args = std::move(boundArgs);
    }
}

void VM::executeInstruction(const Instruction& inst, int ip, int& nextIp, bool& didReturn, Value& returnValue) {
    const std::string& op = inst.opname;

    if (op == "LOAD_CONST") {
        push(constAt(inst));
    } else if (op == "LOAD_IMM8") {
        push(Value::integer(inst.oparg));
    } else if (op == "LOAD_NONE") {
        push(Value::null());
    } else if (op == "LOAD_CONST_VAR" || op == "LOAD_GLOBAL_CONST") {
        const std::string name = optionalNameAt(inst, "__const_" + std::to_string(inst.oparg));
        auto local = currentFrame().constVars.find(name);
        if (local != currentFrame().constVars.end()) {
            push(local->second);
        } else {
            auto global = constGlobals_.find(name);
            if (global == constGlobals_.end()) {
                runtimeError("const variable not found: " + name);
            }
            push(global->second);
        }
    } else if (op == "LOAD_ENV_VAR") {
        const std::string name = optionalNameAt(inst, "__env_" + std::to_string(inst.oparg));
        auto local = currentFrame().envVars.find(name);
        if (local != currentFrame().envVars.end()) {
            push(local->second);
        } else {
            auto global = envGlobals_.find(name);
            if (global == envGlobals_.end()) {
                runtimeError("environment variable not found: " + name);
            }
            push(global->second);
        }
    } else if (op == "LOAD_NAME") {
        push(resolveName(nameAt(inst)));
    } else if (op == "STORE_NAME" || op == "STORE_VAR") {
        currentFrame().locals[nameAt(inst)] = pop();
    } else if (op == "STORE_NAME_CONST" || op == "STORE_CONST_VAR" || op == "STORE_GLOBAL_CONST") {
        const std::string name = optionalNameAt(inst, "__const_" + std::to_string(inst.oparg));
        Value value = pop();
        currentFrame().constVars[name] = value;
        constGlobals_[name] = value;
    } else if (op == "STORE_ENV_VAR") {
        const std::string name = optionalNameAt(inst, "__env_" + std::to_string(inst.oparg));
        Value value = pop();
        currentFrame().envVars[name] = value;
        envGlobals_[name] = value;
    } else if (op == "STORE_SCRIPT" || op == "STORE_SCRIPT_CONST") {
        const std::string name = optionalNameAt(inst, "__script_" + std::to_string(inst.oparg));
        Value value = hasStack(1) ? pop()
            : (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->blocks.size()
                ? Value::function(blockAt(inst))
                : makeMetadataObject("script", {{"name", Value::string(name)}}));
        currentFrame().scripts[name] = value;
        scripts_[name] = value;
        if (op == "STORE_SCRIPT_CONST") {
            currentFrame().constVars[name] = value;
            constGlobals_[name] = value;
        }
    } else if (op == "LOAD_GLOBAL" || op == "LOAD_GVAR") {
        push(resolveGlobal(nameAt(inst)));
    } else if (op == "STORE_GLOBAL" || op == "STORE_GVAR") {
        globals_[nameAt(inst)] = pop();
    } else if (op == "LOAD_VARIABLE" || op == "LOAD_MICRO_VARIABLE") {
        opLoadVariable(inst);
    } else if (op == "STORE_VARIABLE" || op == "STORE_VARIABLE_CONST") {
        opStoreVariable(inst, pop());
    } else if (op == "STORE_TYPE") {
        const std::string name = optionalNameAt(inst, "__type_" + std::to_string(inst.oparg));
        Value value = pop();
        currentFrame().typeVars[name] = value;
        typeGlobals_[name] = value;
    } else if (op == "LOAD_VAR") {
        const std::string& name = nameAt(inst);
        auto it = currentFrame().locals.find(name);
        if (it == currentFrame().locals.end()) {
            runtimeError("NameError: local variable not found: " + name);
        }
        push(it->second);
    } else if (op == "LOAD_TVAR") {
        const std::string& name = nameAt(inst);
        auto it = currentFrame().tvars.find(name);
        if (it == currentFrame().tvars.end()) {
            runtimeError("NameError: t variable not found: " + name);
        }
        push(it->second);
    } else if (op == "STORE_TVAR") {
        currentFrame().tvars[nameAt(inst)] = pop();
    } else if (op == "LOAD_BVAR") {
        const std::string& name = nameAt(inst);
        auto it = currentFrame().bvars.find(name);
        if (it == currentFrame().bvars.end()) {
            runtimeError("NameError: b variable not found: " + name);
        }
        push(it->second);
    } else if (op == "STORE_BVAR") {
        currentFrame().bvars[nameAt(inst)] = pop();
    } else if (op == "LOAD_ARG") {
        if (inst.oparg < 0 || static_cast<size_t>(inst.oparg) >= currentFrame().args.size()) {
            runtimeError("argument index out of range: " + std::to_string(inst.oparg));
        }
        push(currentFrame().args[static_cast<size_t>(inst.oparg)]);
    } else if (op == "LOAD_BUILTIN_FUNC") {
        const std::string& name = nameAt(inst);
        auto it = builtins_.find(name);
        if (it == builtins_.end()) {
            runtimeError("builtin not found: " + name);
        }
        push(it->second);
    } else if (op == "LOAD_FUNCTION") {
        if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->blocks.size()) {
            push(Value::function(blockAt(inst)));
        } else {
            push(resolveName(optionalNameAt(inst, "__function_" + std::to_string(inst.oparg))));
        }
    } else if (op == "LOAD_FUNC_VARG") {
        std::vector<Value> values;
        size_t start = inst.oparg < 0 ? 0 : static_cast<size_t>(inst.oparg);
        for (size_t i = start; i < currentFrame().args.size(); ++i) {
            values.push_back(currentFrame().args[i]);
        }
        push(Value::list(std::move(values)));
    } else if (op == "LOAD_LAMBDA" || op == "MAKE_FUNCTION") {
        if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->blocks.size()) {
            push(Value::function(blockAt(inst)));
        } else if (hasStack(1) && peek().type == ValueType::String) {
            std::string name = pop().toString();
            auto block = findCodeBlock(program_.root, name);
            if (!block) {
                runtimeError("function CodeBlock not found: " + name);
            }
            push(Value::function(block));
        } else if (hasStack(1) && peek().type == ValueType::Function) {
            push(pop());
        } else {
            runtimeError(op + " needs a block index, function name, or function value");
        }
    } else if (op == "LOAD_MODULE") {
        std::string name = optionalNameAt(inst, currentFrame().scopedFqn.empty() ? "__module__" : currentFrame().scopedFqn);
        push(getOrCreateModule(name));
    } else if (op == "LOAD_SCRIPT") {
        const std::string name = optionalNameAt(inst, currentFrame().scopedFqn.empty() ? "__script__" : currentFrame().scopedFqn);
        auto it = scripts_.find(name);
        if (it != scripts_.end()) {
            push(it->second);
        } else if (auto block = findCodeBlock(program_.root, name)) {
            push(Value::function(block));
        } else {
            push(Value::string(name));
        }
    } else if (op == "LOAD_SCRIPT_ID") {
        push(Value::string(currentFrame().scopedFqn.empty() ? currentFrame().frameName : currentFrame().scopedFqn));
    } else if (op == "LOAD_SENSE_PARSER") {
        push(makeMetadataObject("sense_parser", {{"name", Value::string(optionalNameAt(inst, "sense"))}}));
    } else if (op == "LOAD_GADGET_FUNC") {
        const std::string name = optionalNameAt(inst, "__gadget_" + std::to_string(inst.oparg));
        auto builtin = builtins_.find(name);
        push(builtin == builtins_.end() ? makeMetadataObject("gadget_func", {{"name", Value::string(name)}}) : builtin->second);
    } else if (op == "LOAD_NAMED_PARAM" || op == "LOAD_NAMED_PARAM_CONST") {
        push(makeMetadataObject("named_param", {{"name", Value::string(optionalNameAt(inst, "__param_" + std::to_string(inst.oparg)))}}));
    } else if (op == "LOAD_NOTE" || op == "LOAD_ANNOTATION") {
        Value name = hasStack(1) ? pop() : Value::string(optionalNameAt(inst, "__note_" + std::to_string(inst.oparg)));
        push(makeMetadataObject("annotation", {{"name", Value::string(name.toString())}}));
    } else if (op == "LOAD_CLASS_META") {
        Value value = pop();
        if (value.type != ValueType::ClassType) {
            runtimeError("LOAD_CLASS_META expected class, got " + value.repr());
        }
        auto klass = std::get<std::shared_ptr<ClassValue>>(value.data);
        push(makeMetadataObject("class_meta", {{"name", Value::string(klass->codeblock ? klass->codeblock->cbname : "")}}));
    } else if (op == "LOAD_INHERIT") {
        auto& inherits = currentFrame().codeblock->inherits;
        if (inst.oparg < 0 || static_cast<size_t>(inst.oparg) >= inherits.size()) {
            runtimeError("inherit index out of range: " + std::to_string(inst.oparg));
        }
        push(Value::string(inherits[static_cast<size_t>(inst.oparg)]));
    } else if (op == "LOAD_FLAW") {
        if (pendingError_.type != ValueType::Null) {
            push(pendingError_);
        } else {
            push(Value::error(optionalNameAt(inst, "Flaw")));
        }
    } else if (op == "LOAD_IMPLICIT_VARIABLE") {
        opLoadImplicitVariable(inst);
    } else if (isFunctionDefineOpcode(op)) {
        push(Value::function(blockAt(inst)));
    } else if (op == "DEFINE_CLASS") {
        push(Value::classType(blockAt(inst)));
    } else if (op == "ALLOC_CLASS_TYPE") {
        if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->blocks.size()) {
            push(Value::classType(blockAt(inst)));
        } else {
            push(Value::classType(currentFrame().codeblock));
        }
    } else if (op == "ADD_MEMORY_CLASS" || op == "STORE_INHERIT") {
        globals_[optionalNameAt(inst, "__class_" + std::to_string(inst.oparg))] = pop();
    } else if (op == "DEFINE_MODULE") {
        Value name = popOrNull();
        push(getOrCreateModule(name.type == ValueType::Null ? optionalNameAt(inst, currentFrame().frameName) : name.toString()));
    } else if (op == "DEFINE_ANNOTATION") {
        Value value = popOrNull();
        push(makeMetadataObject("annotation", {{"value", value}}));
    } else if (op == "DEFINE_FLAW") {
        Value payload = popOrNull();
        Value name = popOrNull();
        push(Value::error(name.type == ValueType::Null ? optionalNameAt(inst, "Flaw") : name.toString(), payload));
    } else if (op == "DECLARE_PACKAGE") {
        Value packageName = popOrNull();
        if (packageName.type == ValueType::Null && inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->constants.size()) {
            packageName = constAt(inst);
        }
        currentFrame().scopedFqn = packageName.toString();
        globals_["__package__"] = packageName;
    } else if (op == "DECLARE_IMPORT" || op == "SOURCE_MODULE") {
        Value moduleName = popOrNull();
        if (moduleName.type == ValueType::Null) {
            moduleName = Value::string(optionalNameAt(inst, "__import_" + std::to_string(inst.oparg)));
        }
        loadedModules_.insert(moduleName.toString());
        globals_[moduleName.toString()] = getOrCreateModule(moduleName.toString());
    } else if (op == "UNLOAD_MODULE") {
        Value moduleName = popOrNull();
        std::string name = moduleName.type == ValueType::Null ? optionalNameAt(inst, "__module_" + std::to_string(inst.oparg)) : moduleName.toString();
        loadedModules_.erase(name);
        globals_.erase(name);
        modules_.erase(name);
    } else if (op == "CREATE_INSTANCE") {
        Value klass = pop();
        if (klass.type != ValueType::ClassType) {
            runtimeError("CREATE_INSTANCE expected class, got " + klass.repr());
        }
        push(Value::instance(std::get<std::shared_ptr<ClassValue>>(klass.data)));
    } else if (op == "LOAD_ATTR") {
        opLoadAttr(nameAt(inst));
    } else if (op == "STORE_ATTR") {
        opStoreAttr(nameAt(inst));
    } else if (op == "UNLET_ATTR") {
        opUnletAttr(optionalNameAt(inst, "__attr_" + std::to_string(inst.oparg)));
    } else if (op == "CALL") {
        opCall(inst.oparg);
    } else if (op == "DEFER_CALL" || op == "DEFER") {
        if (inst.oparg < 0) {
            runtimeError(op + " arg count cannot be negative");
        }
        std::vector<Value> args;
        for (int i = 0; i < inst.oparg; ++i) {
            args.push_back(pop());
        }
        std::reverse(args.begin(), args.end());
        Value callable = popOrNull();
        push(makeMetadataObject("deferred_call", {{"callable", Value::string(callable.repr())}, {"args", Value::list(std::move(args))}}));
    } else if (op == "TENNIS_CALL") {
        opCall(inst.oparg);
    } else if (op == "RETURN_VALUE") {
        returnValue = currentFrame().stack.empty() ? Value::null() : pop();
        didReturn = true;
    } else if (op == "SCRIPT_FINISH") {
        returnValue = Value::null();
        didReturn = true;
    } else if (op == "YIELD_VALUE") {
        Value yielded = currentFrame().stack.empty() ? Value::null() : pop();
        currentFrame().yields.push_back(yielded);
        push(yielded);
    } else if (op == "RETURN_GENERATOR") {
        returnValue = Value::iterator(currentFrame().yields);
        didReturn = true;
    } else if (op == "BINARY_OP") {
        opBinary(inst.oparg);
    } else if (op == "UNARY_OP") {
        opUnary(inst.oparg);
    } else if (op == "COMPARE_OP") {
        opCompare(inst.oparg);
    } else if (op == "BUILD_LIST") {
        if (inst.oparg < 0) {
            runtimeError("BUILD_LIST count cannot be negative");
        }
        std::vector<Value> values;
        for (int i = 0; i < inst.oparg; ++i) {
            values.push_back(pop());
        }
        std::reverse(values.begin(), values.end());
        push(Value::list(std::move(values)));
    } else if (op == "BUILD_DICT") {
        if (inst.oparg < 0) {
            runtimeError("BUILD_DICT count cannot be negative");
        }
        std::unordered_map<std::string, Value> values;
        for (int i = 0; i < inst.oparg; ++i) {
            Value value = pop();
            Value key = pop();
            values[key.dictKey()] = value;
        }
        push(Value::dict(std::move(values)));
    } else if (op == "LOAD_SUBSCR") {
        opLoadSubscr();
    } else if (op == "STORE_SUBSCR") {
        opStoreSubscr();
    } else if (op == "UNLET_SUBSCR") {
        opUnletSubscr();
    } else if (op == "GET_ITER") {
        opGetIter();
    } else if (op == "FOR_ITER") {
        opForIter(inst, ip, nextIp);
    } else if (op == "SLICE") {
        opSlice();
    } else if (op == "UNPACK_LIST" || op == "POP_UNPACK") {
        opUnpackList(inst.oparg);
    } else if (op == "UNPACK_DICT") {
        opUnpackDict(inst.oparg);
    } else if (op == "POP_TOP") {
        (void)pop();
    } else if (op == "POP_LOAD") {
        (void)pop();
        push(resolveName(nameAt(inst)));
    } else if (op == "ECHO") {
        std::cout << pop().toString() << "\n";
    } else if (op == "TO_BOOL") {
        push(Value::boolean(pop().truthy()));
    } else if (op == "TO_STRING") {
        push(Value::string(pop().toString()));
    } else if (op == "TO_PREDICATE") {
        push(Value::boolean(pop().truthy()));
    } else if (op == "TYPE_CAST") {
        opTypeCast(inst);
    } else if (op == "JUMP_FORWARD" || op == "JUMP_BACKWARD") {
        nextIp = jumpTarget(inst, ip);
    } else if (op == "BREAK" || op == "CONTINUE_LOOP") {
        if (inst.resolvedTarget >= 0 || inst.oparg != 0) {
            nextIp = jumpTarget(inst, ip);
        } else {
            runtimeError(op + " needs target/oparg in metadata");
        }
    } else if (op == "EXTENDED_ARG") {
        if (hasStack(1)) {
            Value value = pop();
            push(makeMetadataObject("extended_arg", {{"value", value}}));
        }
    } else if (op == "ENTER_LOOP" || op == "LEAVE_LOOP" || op == "EXIT_SCOPE" || op == "GARBAGE_COLLECT") {
        // Training VM bookkeeping/no-op opcode.
    } else if (op == "POP_JUMP_IF_FALSE") {
        if (!pop().truthy()) {
            nextIp = jumpTarget(inst, ip);
        }
    } else if (op == "POP_JUMP_IF_TRUE") {
        if (pop().truthy()) {
            nextIp = jumpTarget(inst, ip);
        }
    } else if (op == "JUMP_IF_FALSE_OR_POP") {
        if (!peek().truthy()) {
            nextIp = jumpTarget(inst, ip);
        } else {
            (void)pop();
        }
    } else if (op == "JUMP_IF_TRUE_OR_POP") {
        if (peek().truthy()) {
            nextIp = jumpTarget(inst, ip);
        } else {
            (void)pop();
        }
    } else if (op == "THROW") {
        opThrow(inst, ip, nextIp);
    } else if (op == "CATCH") {
        push(pendingError_);
    } else if (op == "REPLACE_FUNCTION") {
        push(Value::function(blockAt(inst)));
    } else if (op == "BIND_NAMED_PARAM") {
        Value value = pop();
        std::string name = optionalNameAt(inst, "__param_" + std::to_string(inst.oparg));
        if (hasStack(1) && isMetadataKind(peek(), "named_param")) {
            Value param = pop();
            name = metadataFieldString(param, "name", name);
        } else if (hasStack(1) && peek().type == ValueType::String && inst.oparg < 0) {
            name = pop().toString();
        }
        push(makeMetadataObject("named_param", {{"name", Value::string(name)}, {"value", value}}));
    } else if (op == "PARSE_MODULE_ATTRS") {
        Value attrs = popOrNull();
        push(makeMetadataObject("module_attrs", {{"source", attrs}}));
    } else if (op == "SET_FQN") {
        Value value = popOrNull();
        currentFrame().scopedFqn = value.type == ValueType::Null ? optionalNameAt(inst, currentFrame().frameName) : value.toString();
    } else if (op == "UNSET_FQN") {
        currentFrame().scopedFqn.clear();
    } else if (op == "GET_PATH_BY_FQN") {
        Value value = hasStack(1) ? pop() : Value::string(optionalNameAt(inst, currentFrame().scopedFqn));
        push(Value::string(fqnToPath(value.toString())));
    } else if (op == "GET_FQN_BY_PATH") {
        Value value = popOrNull();
        std::string fqn = value.toString();
        std::replace(fqn.begin(), fqn.end(), '/', '.');
        push(Value::string(fqn));
    } else if (op == "GET_CALLER_MODULE") {
        std::string caller = frames_.size() >= 2 ? frames_[frames_.size() - 2].scopedFqn : currentFrame().scopedFqn;
        push(getOrCreateModule(caller.empty() ? "<main>" : caller));
    } else if (op == "EXECUTE") {
        Value command = popOrNull();
        push(makeMetadataObject("execute_command", {{"command", command}, {"executed", Value::boolean(false)}}));
    } else if (op == "SOURCE") {
        Value scriptName = popOrNull();
        std::string name = scriptName.type == ValueType::Null ? optionalNameAt(inst, "__source_" + std::to_string(inst.oparg)) : scriptName.toString();
        auto block = findCodeBlock(program_.root, name);
        if (block) {
            push(executeBlock(block, {}));
        } else {
            loadedModules_.insert(name);
            push(getOrCreateModule(name));
        }
    } else if (op == "EXEC_NETABLOCK") {
        opExecNetaBlock(inst);
    } else if (op == "BUILD_GENERICS") {
        opBuildGenerics(inst.oparg);
    } else if (op == "BUILD_RECORD") {
        opBuildRecord(inst.oparg);
    } else if (op == "GENERICS_INSTANCE") {
        opGenericsInstance();
    } else if (op == "INSTANCE_OF") {
        Value klass = pop();
        Value value = pop();
        bool ok = false;
        if (klass.type == ValueType::ClassType && value.type == ValueType::Instance) {
            ok = std::get<std::shared_ptr<InstanceValue>>(value.data)->klass == std::get<std::shared_ptr<ClassValue>>(klass.data);
        } else {
            ok = valueTypeName(value.type) == klass.toString() || value.repr() == klass.toString();
        }
        push(Value::boolean(ok));
    } else if (op == "INSTANCE_NOT") {
        Value klass = pop();
        Value value = pop();
        bool ok = false;
        if (klass.type == ValueType::ClassType && value.type == ValueType::Instance) {
            ok = std::get<std::shared_ptr<InstanceValue>>(value.data)->klass == std::get<std::shared_ptr<ClassValue>>(klass.data);
        } else {
            ok = valueTypeName(value.type) == klass.toString() || value.repr() == klass.toString();
        }
        push(Value::boolean(!ok));
    } else if (op == "UNLET_NAME") {
        currentFrame().locals.erase(optionalNameAt(inst, "__name_" + std::to_string(inst.oparg)));
    } else if (op == "UNLET_GLOBAL") {
        globals_.erase(optionalNameAt(inst, "__global_" + std::to_string(inst.oparg)));
    } else if (op == "UNLET_CONST_VAR") {
        const std::string name = optionalNameAt(inst, "__const_" + std::to_string(inst.oparg));
        currentFrame().constVars.erase(name);
        constGlobals_.erase(name);
    } else if (op == "UNLET_SCRIPT") {
        const std::string name = optionalNameAt(inst, "__script_" + std::to_string(inst.oparg));
        currentFrame().scripts.erase(name);
        scripts_.erase(name);
    } else if (op == "UNLET_VARIABLE") {
        const std::string name = optionalNameAt(inst, "__var_" + std::to_string(inst.oparg));
        ScopedName scoped = splitScopedName(name);
        const std::string& actual = scoped.scope.empty() ? name : scoped.name;
        if (scoped.scope.empty() || scopeMatches(scoped.scope, {"local", "name", "l"})) {
            currentFrame().locals.erase(actual);
        }
        if (scoped.scope.empty() || scopeMatches(scoped.scope, {"const", "c"})) {
            currentFrame().constVars.erase(actual);
            constGlobals_.erase(actual);
        }
        if (scoped.scope.empty() || scopeMatches(scoped.scope, {"env", "e"})) {
            currentFrame().envVars.erase(actual);
            envGlobals_.erase(actual);
        }
        if (scoped.scope.empty() || scopeMatches(scoped.scope, {"type"})) {
            currentFrame().typeVars.erase(actual);
            typeGlobals_.erase(actual);
        }
        if (scoped.scope.empty() || scopeMatches(scoped.scope, {"script", "s"})) {
            currentFrame().scripts.erase(actual);
            scripts_.erase(actual);
        }
        if (scoped.scope.empty() || scopeMatches(scoped.scope, {"global", "span", "g"})) {
            globals_.erase(actual);
        }
    } else if (op == "SLEEP") {
        if (inst.oparg > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(inst.oparg));
        }
    } else if (op == "MAKE_MUTABLE") {
        push(pop());
    } else if (op == "CLASS_STATIC_ANNOTATION" || op == "FUNC_STATIC_ANNOTATION") {
        currentFrame().locals["__last_static_annotation__"] = popOrNull();
    } else if (op == "OPCODE_NUM") {
        push(Value::integer(static_cast<int64_t>(registry_.specs().size())));
    } else if (op == "OPCODE_ILL") {
        runtimeError("illegal opcode");
    } else {
        runtimeError("opcode not implemented: " + op);
    }
}

Frame& VM::currentFrame() {
    if (frames_.empty()) {
        throw RuntimeError("internal error: no current frame");
    }
    return frames_.back();
}

const Frame& VM::currentFrame() const {
    if (frames_.empty()) {
        throw RuntimeError("internal error: no current frame");
    }
    return frames_.back();
}

Value VM::pop() {
    auto& stack = currentFrame().stack;
    if (stack.empty()) {
        runtimeError("stack underflow");
    }
    Value value = stack.back();
    stack.pop_back();
    return value;
}

Value VM::peek() const {
    const auto& stack = currentFrame().stack;
    if (stack.empty()) {
        runtimeError("stack underflow");
    }
    return stack.back();
}

void VM::push(Value value) {
    currentFrame().stack.push_back(std::move(value));
}

bool VM::hasStack(size_t count) const {
    return currentFrame().stack.size() >= count;
}

Value VM::popOrNull() {
    if (!hasStack(1)) {
        return Value::null();
    }
    return pop();
}

Value VM::getOrCreateModule(const std::string& name) {
    auto found = modules_.find(name);
    if (found != modules_.end()) {
        return found->second;
    }
    Value module = Value::module(name);
    modules_[name] = module;
    return module;
}

const Value* VM::metadataField(const Value& value, const std::string& field) const {
    if (value.type != ValueType::Dict) {
        return nullptr;
    }
    const auto& items = std::get<std::shared_ptr<DictValue>>(value.data)->items;
    auto found = items.find(field);
    if (found == items.end()) {
        return nullptr;
    }
    return &found->second;
}

bool VM::isMetadataKind(const Value& value, const std::string& kind) const {
    const Value* field = metadataField(value, "__kind");
    return field && field->toString() == kind;
}

std::string VM::metadataFieldString(const Value& value, const std::string& field, const std::string& fallback) const {
    const Value* found = metadataField(value, field);
    return found ? found->toString() : fallback;
}

std::vector<Value> VM::collectCallArguments(int argCount, std::unordered_map<std::string, Value>& namedArgs) {
    if (argCount < 0) {
        runtimeError("CALL arg count cannot be negative");
    }

    std::vector<Value> rawArgs;
    rawArgs.reserve(static_cast<size_t>(argCount));
    for (int i = 0; i < argCount; ++i) {
        rawArgs.push_back(pop());
    }
    std::reverse(rawArgs.begin(), rawArgs.end());

    std::vector<Value> args;
    for (const auto& arg : rawArgs) {
        if (isMetadataKind(arg, "named_param")) {
            const Value* value = metadataField(arg, "value");
            if (!value) {
                runtimeError("named parameter is missing value");
            }
            std::string name = metadataFieldString(arg, "name", "");
            if (name.empty()) {
                runtimeError("named parameter is missing name");
            }
            if (namedArgs.find(name) != namedArgs.end()) {
                runtimeError("duplicate named parameter: " + name);
            }
            namedArgs[name] = *value;
            continue;
        }

        if (isMetadataKind(arg, "extended_arg")) {
            const Value* value = metadataField(arg, "value");
            if (!value) {
                continue;
            }
            if (value->type == ValueType::List) {
                const auto& items = std::get<std::shared_ptr<ListValue>>(value->data)->items;
                args.insert(args.end(), items.begin(), items.end());
                continue;
            }
            if (value->type == ValueType::Dict) {
                const auto& items = std::get<std::shared_ptr<DictValue>>(value->data)->items;
                for (const auto& item : items) {
                    if (namedArgs.find(item.first) != namedArgs.end()) {
                        runtimeError("duplicate named parameter: " + item.first);
                    }
                    namedArgs[item.first] = item.second;
                }
                continue;
            }
            args.push_back(*value);
            continue;
        }

        args.push_back(arg);
    }
    return args;
}

Value VM::resolveName(const std::string& name) {
    ScopedName scoped = splitScopedName(name);
    if (!scoped.scope.empty()) {
        const std::string& scope = scoped.scope;
        const std::string& actual = scoped.name;
        if (scopeMatches(scope, {"local", "name", "l"})) {
            auto found = currentFrame().locals.find(actual);
            if (found != currentFrame().locals.end()) {
                return found->second;
            }
        } else if (scopeMatches(scope, {"global", "span", "g"})) {
            auto found = globals_.find(actual);
            if (found != globals_.end()) {
                return found->second;
            }
        } else if (scopeMatches(scope, {"const", "c"})) {
            auto local = currentFrame().constVars.find(actual);
            if (local != currentFrame().constVars.end()) {
                return local->second;
            }
            auto global = constGlobals_.find(actual);
            if (global != constGlobals_.end()) {
                return global->second;
            }
        } else if (scopeMatches(scope, {"env", "e"})) {
            auto local = currentFrame().envVars.find(actual);
            if (local != currentFrame().envVars.end()) {
                return local->second;
            }
            auto global = envGlobals_.find(actual);
            if (global != envGlobals_.end()) {
                return global->second;
            }
        } else if (scopeMatches(scope, {"type"})) {
            auto local = currentFrame().typeVars.find(actual);
            if (local != currentFrame().typeVars.end()) {
                return local->second;
            }
            auto global = typeGlobals_.find(actual);
            if (global != typeGlobals_.end()) {
                return global->second;
            }
        } else if (scopeMatches(scope, {"script", "s"})) {
            auto local = currentFrame().scripts.find(actual);
            if (local != currentFrame().scripts.end()) {
                return local->second;
            }
            auto global = scripts_.find(actual);
            if (global != scripts_.end()) {
                return global->second;
            }
        } else if (scopeMatches(scope, {"module", "m"})) {
            return getOrCreateModule(actual);
        }
        runtimeError("NameError: scoped name not found: " + name);
    }

    auto local = currentFrame().locals.find(name);
    if (local != currentFrame().locals.end()) {
        return local->second;
    }
    auto constLocal = currentFrame().constVars.find(name);
    if (constLocal != currentFrame().constVars.end()) {
        return constLocal->second;
    }
    auto envLocal = currentFrame().envVars.find(name);
    if (envLocal != currentFrame().envVars.end()) {
        return envLocal->second;
    }
    auto typeLocal = currentFrame().typeVars.find(name);
    if (typeLocal != currentFrame().typeVars.end()) {
        return typeLocal->second;
    }
    auto scriptLocal = currentFrame().scripts.find(name);
    if (scriptLocal != currentFrame().scripts.end()) {
        return scriptLocal->second;
    }
    auto global = globals_.find(name);
    if (global != globals_.end()) {
        return global->second;
    }
    auto constGlobal = constGlobals_.find(name);
    if (constGlobal != constGlobals_.end()) {
        return constGlobal->second;
    }
    auto envGlobal = envGlobals_.find(name);
    if (envGlobal != envGlobals_.end()) {
        return envGlobal->second;
    }
    auto typeGlobal = typeGlobals_.find(name);
    if (typeGlobal != typeGlobals_.end()) {
        return typeGlobal->second;
    }
    auto scriptGlobal = scripts_.find(name);
    if (scriptGlobal != scripts_.end()) {
        return scriptGlobal->second;
    }
    auto module = modules_.find(name);
    if (module != modules_.end()) {
        return module->second;
    }
    auto builtin = builtins_.find(name);
    if (builtin != builtins_.end()) {
        return builtin->second;
    }
    runtimeError("NameError: name not found: " + name);
}

Value VM::resolveGlobal(const std::string& name) {
    auto global = globals_.find(name);
    if (global != globals_.end()) {
        return global->second;
    }
    auto constGlobal = constGlobals_.find(name);
    if (constGlobal != constGlobals_.end()) {
        return constGlobal->second;
    }
    auto envGlobal = envGlobals_.find(name);
    if (envGlobal != envGlobals_.end()) {
        return envGlobal->second;
    }
    auto typeGlobal = typeGlobals_.find(name);
    if (typeGlobal != typeGlobals_.end()) {
        return typeGlobal->second;
    }
    auto scriptGlobal = scripts_.find(name);
    if (scriptGlobal != scripts_.end()) {
        return scriptGlobal->second;
    }
    auto module = modules_.find(name);
    if (module != modules_.end()) {
        return module->second;
    }
    auto builtin = builtins_.find(name);
    if (builtin != builtins_.end()) {
        return builtin->second;
    }
    runtimeError("NameError: global name not found: " + name);
}

const std::string& VM::nameAt(const Instruction& inst) {
    auto& names = currentFrame().codeblock->names;
    if (inst.oparg < 0 || static_cast<size_t>(inst.oparg) >= names.size()) {
        runtimeError("name index out of range: " + std::to_string(inst.oparg));
    }
    return names[static_cast<size_t>(inst.oparg)];
}

std::string VM::optionalNameAt(const Instruction& inst, const std::string& fallback) const {
    auto& names = currentFrame().codeblock->names;
    if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < names.size()) {
        return names[static_cast<size_t>(inst.oparg)];
    }
    return fallback;
}

Value VM::constAt(const Instruction& inst) {
    auto& constants = currentFrame().codeblock->constants;
    if (inst.oparg < 0 || static_cast<size_t>(inst.oparg) >= constants.size()) {
        runtimeError("constant index out of range: " + std::to_string(inst.oparg));
    }
    return constants[static_cast<size_t>(inst.oparg)];
}

std::shared_ptr<CodeBlock> VM::blockAt(const Instruction& inst) {
    auto& blocks = currentFrame().codeblock->blocks;
    if (inst.oparg < 0 || static_cast<size_t>(inst.oparg) >= blocks.size()) {
        runtimeError("block index out of range: " + std::to_string(inst.oparg));
    }
    return blocks[static_cast<size_t>(inst.oparg)];
}

void VM::opBinary(int op) {
    Value right = pop();
    Value left = pop();

    switch (op) {
        case 1: {
            if (left.type == ValueType::String || right.type == ValueType::String) {
                push(Value::string(left.toString() + right.toString()));
            } else if (left.isNumeric() && right.isNumeric()) {
                if (shouldReturnInt(left, right)) {
                    push(Value::integer(left.asInt() + right.asInt()));
                } else {
                    push(Value::number(left.asDouble() + right.asDouble()));
                }
            } else {
                runtimeError("unsupported operands for NB_ADD: " + left.repr() + ", " + right.repr());
            }
            break;
        }
        case 2:
            if (!left.isNumeric() || !right.isNumeric()) {
                runtimeError("unsupported operands for NB_SUBSTRACT: " + left.repr() + ", " + right.repr());
            }
            if (shouldReturnInt(left, right)) {
                push(Value::integer(left.asInt() - right.asInt()));
            } else {
                push(Value::number(left.asDouble() - right.asDouble()));
            }
            break;
        case 3:
            if (!left.isNumeric() || !right.isNumeric()) {
                runtimeError("unsupported operands for NB_MULTIPLY: " + left.repr() + ", " + right.repr());
            }
            if (shouldReturnInt(left, right)) {
                push(Value::integer(left.asInt() * right.asInt()));
            } else {
                push(Value::number(left.asDouble() * right.asDouble()));
            }
            break;
        case 4:
            if (!left.isNumeric() || !right.isNumeric()) {
                runtimeError("unsupported operands for NB_DEVIDE: " + left.repr() + ", " + right.repr());
            }
            if (right.asDouble() == 0.0) {
                runtimeError("division by zero");
            }
            push(Value::number(left.asDouble() / right.asDouble()));
            break;
        case 5:
            if (!left.isNumeric() || !right.isNumeric()) {
                runtimeError("unsupported operands for NB_MOD: " + left.repr() + ", " + right.repr());
            }
            if (right.asInt() == 0) {
                runtimeError("modulo by zero");
            }
            push(Value::integer(left.asInt() % right.asInt()));
            break;
        case 7:
            push(Value::boolean(left.truthy() && right.truthy()));
            break;
        case 6:
        case 14:
            requireInts(left, right, binaryOpName(op));
            push(Value::integer(left.asInt() & right.asInt()));
            break;
        case 8:
        case 15:
            requireInts(left, right, binaryOpName(op));
            push(Value::integer(left.asInt() | right.asInt()));
            break;
        case 9:
            push(Value::boolean(left.truthy() || right.truthy()));
            break;
        case 10:
        case 16:
            requireInts(left, right, binaryOpName(op));
            push(Value::integer(left.asInt() ^ right.asInt()));
            break;
        case 11:
            push(Value::boolean(left.truthy() != right.truthy()));
            break;
        case 12:
        case 17:
            requireInts(left, right, binaryOpName(op));
            push(Value::integer(left.asInt() << right.asInt()));
            break;
        case 13:
        case 18:
            requireInts(left, right, binaryOpName(op));
            push(Value::integer(left.asInt() >> right.asInt()));
            break;
        default:
            runtimeError("binary op not implemented: " + binaryOpName(op));
    }
}

void VM::opUnary(int op) {
    Value value = pop();
    switch (op) {
        case 1:
            if (!value.isNumeric()) {
                runtimeError("unsupported operand for UOP_PLUS: " + value.repr());
            }
            push(value);
            break;
        case 2:
            if (!value.isNumeric()) {
                runtimeError("unsupported operand for UOP_MINUS: " + value.repr());
            }
            if (value.type == ValueType::Int) {
                push(Value::integer(-value.asInt()));
            } else {
                push(Value::number(-value.asDouble()));
            }
            break;
        case 3:
        case 5:
            if (value.type != ValueType::Int) {
                runtimeError("unsupported operand for " + unaryOpName(op) + ": " + value.repr());
            }
            push(Value::integer(~value.asInt()));
            break;
        case 4:
            push(Value::boolean(!value.truthy()));
            break;
        default:
            runtimeError("unary op not implemented: " + unaryOpName(op));
    }
}

void VM::opCompare(int op) {
    Value right = pop();
    Value left = pop();
    switch (op) {
        case 1:
            push(Value::boolean(compareValues(left, right) < 0));
            break;
        case 2:
            push(Value::boolean(compareValues(left, right) <= 0));
            break;
        case 3:
            push(Value::boolean(valueEquals(left, right)));
            break;
        case 4:
            push(Value::boolean(!valueEquals(left, right)));
            break;
        case 5:
            push(Value::boolean(compareValues(left, right) > 0));
            break;
        case 6:
            push(Value::boolean(compareValues(left, right) >= 0));
            break;
        case 7:
            push(Value::boolean(patternMatch(left, right)));
            break;
        case 8:
            push(Value::boolean(patternMatch(right, left)));
            break;
        case 9:
            push(Value::boolean(valueEquals(left, right)));
            break;
        case 10:
            push(Value::boolean(!valueEquals(left, right)));
            break;
        default:
            runtimeError("compare op not implemented: " + compareOpName(op));
    }
}

void VM::opCall(int argCount) {
    std::unordered_map<std::string, Value> namedArgs;
    std::vector<Value> args = collectCallArguments(argCount, namedArgs);
    Value callable = pop();
    if (callable.type == ValueType::Function) {
        auto fn = std::get<std::shared_ptr<FunctionValue>>(callable.data);
        if (!fn || !fn->codeblock) {
            runtimeError("attempted to call null function");
        }
        Value result = executeBlock(fn->codeblock, args, namedArgs);
        push(result);
        return;
    }
    if (callable.type == ValueType::BuiltinFunction) {
        if (!namedArgs.empty()) {
            runtimeError("builtin functions do not accept named arguments in this VM");
        }
        auto builtin = std::get<std::shared_ptr<BuiltinValue>>(callable.data);
        Value result = builtin->fn(args);
        push(result);
        return;
    }
    if (callable.type == ValueType::ClassType) {
        auto klass = std::get<std::shared_ptr<ClassValue>>(callable.data);
        Value instance = Value::instance(klass);
        auto init = klass->attrs.find("__init__");
        if (init == klass->attrs.end()) {
            init = klass->attrs.find("init");
        }
        if (init != klass->attrs.end() && init->second.type == ValueType::Function) {
            std::vector<Value> initArgs;
            initArgs.reserve(args.size() + 1);
            initArgs.push_back(instance);
            initArgs.insert(initArgs.end(), args.begin(), args.end());
            auto fn = std::get<std::shared_ptr<FunctionValue>>(init->second.data);
            (void)executeBlock(fn->codeblock, initArgs, namedArgs);
        }
        push(instance);
        return;
    }

    runtimeError("CALL expected function, got " + callable.repr());
}

void VM::opLoadSubscr() {
    Value index = pop();
    Value container = pop();
    if (container.type == ValueType::List) {
        const auto& items = std::get<std::shared_ptr<ListValue>>(container.data)->items;
        int64_t idx = normalizeIndex(index.asInt(), items.size());
        if (idx < 0 || static_cast<size_t>(idx) >= items.size()) {
            runtimeError("list index out of range: " + std::to_string(idx));
        }
        push(items[static_cast<size_t>(idx)]);
        return;
    }
    if (container.type == ValueType::Dict) {
        const auto& items = std::get<std::shared_ptr<DictValue>>(container.data)->items;
        auto it = items.find(index.dictKey());
        if (it == items.end()) {
            runtimeError("dict key not found: " + index.dictKey());
        }
        push(it->second);
        return;
    }
    if (container.type == ValueType::String) {
        const auto& text = std::get<std::string>(container.data);
        int64_t idx = normalizeIndex(index.asInt(), text.size());
        if (idx < 0 || static_cast<size_t>(idx) >= text.size()) {
            runtimeError("string index out of range: " + std::to_string(idx));
        }
        push(Value::string(std::string(1, text[static_cast<size_t>(idx)])));
        return;
    }
    runtimeError("LOAD_SUBSCR unsupported for " + container.repr());
}

void VM::opStoreSubscr() {
    Value value = pop();
    Value index = pop();
    Value container = pop();
    if (container.type == ValueType::List) {
        auto items = std::get<std::shared_ptr<ListValue>>(container.data);
        int64_t idx = normalizeIndex(index.asInt(), items->items.size());
        if (idx < 0 || static_cast<size_t>(idx) >= items->items.size()) {
            runtimeError("list index out of range: " + std::to_string(idx));
        }
        items->items[static_cast<size_t>(idx)] = value;
        return;
    }
    if (container.type == ValueType::Dict) {
        auto items = std::get<std::shared_ptr<DictValue>>(container.data);
        items->items[index.dictKey()] = value;
        return;
    }
    runtimeError("STORE_SUBSCR unsupported for " + container.repr());
}

void VM::opLoadAttr(const std::string& name) {
    Value obj = pop();
    if (name == "len" || name == "length" || name == "size") {
        if (obj.type == ValueType::List) {
            push(Value::integer(static_cast<int64_t>(std::get<std::shared_ptr<ListValue>>(obj.data)->items.size())));
            return;
        }
        if (obj.type == ValueType::Dict) {
            push(Value::integer(static_cast<int64_t>(std::get<std::shared_ptr<DictValue>>(obj.data)->items.size())));
            return;
        }
        if (obj.type == ValueType::String) {
            push(Value::integer(static_cast<int64_t>(std::get<std::string>(obj.data).size())));
            return;
        }
        if (obj.type == ValueType::Iterator) {
            push(Value::integer(static_cast<int64_t>(std::get<std::shared_ptr<IteratorValue>>(obj.data)->items.size())));
            return;
        }
    }
    if (obj.type == ValueType::Instance) {
        auto instanceValue = std::get<std::shared_ptr<InstanceValue>>(obj.data);
        auto it = instanceValue->attrs.find(name);
        if (it != instanceValue->attrs.end()) {
            push(it->second);
            return;
        }
        if (instanceValue->klass) {
            auto classIt = instanceValue->klass->attrs.find(name);
            if (classIt != instanceValue->klass->attrs.end()) {
                push(classIt->second);
                return;
            }
        }
        runtimeError("attribute not found: " + name);
    }
    if (obj.type == ValueType::ClassType) {
        auto klass = std::get<std::shared_ptr<ClassValue>>(obj.data);
        if (name == "name") {
            push(Value::string(klass->codeblock ? klass->codeblock->cbname : ""));
            return;
        }
        auto it = klass->attrs.find(name);
        if (it == klass->attrs.end()) {
            runtimeError("class attribute not found: " + name);
        }
        push(it->second);
        return;
    }
    if (obj.type == ValueType::Module) {
        auto moduleValue = std::get<std::shared_ptr<ModuleValue>>(obj.data);
        if (name == "name") {
            push(Value::string(moduleValue->name));
            return;
        }
        auto it = moduleValue->attrs.find(name);
        if (it == moduleValue->attrs.end()) {
            runtimeError("module attribute not found: " + name);
        }
        push(it->second);
        return;
    }
    if (obj.type == ValueType::Dict) {
        const auto& items = std::get<std::shared_ptr<DictValue>>(obj.data)->items;
        auto it = items.find(name);
        if (it == items.end()) {
            runtimeError("dict attribute/key not found: " + name);
        }
        push(it->second);
        return;
    }
    if (obj.type == ValueType::Function && name == "name") {
        const auto& fn = std::get<std::shared_ptr<FunctionValue>>(obj.data);
        push(Value::string(fn->codeblock ? fn->codeblock->cbname : ""));
        return;
    }
    if (obj.type == ValueType::BuiltinFunction && name == "name") {
        push(Value::string(std::get<std::shared_ptr<BuiltinValue>>(obj.data)->name));
        return;
    }
    if (obj.type == ValueType::Error) {
        const auto& error = std::get<std::shared_ptr<ErrorValue>>(obj.data);
        if (name == "name") {
            push(Value::string(error->name));
            return;
        }
        if (name == "payload") {
            push(error->payload);
            return;
        }
    }
    runtimeError("LOAD_ATTR unsupported for " + obj.repr());
}

void VM::opStoreAttr(const std::string& name) {
    Value value = pop();
    Value obj = pop();
    if (obj.type == ValueType::Instance) {
        auto instanceValue = std::get<std::shared_ptr<InstanceValue>>(obj.data);
        instanceValue->attrs[name] = value;
        return;
    }
    if (obj.type == ValueType::ClassType) {
        auto klass = std::get<std::shared_ptr<ClassValue>>(obj.data);
        klass->attrs[name] = value;
        return;
    }
    if (obj.type == ValueType::Module) {
        auto moduleValue = std::get<std::shared_ptr<ModuleValue>>(obj.data);
        moduleValue->attrs[name] = value;
        return;
    }
    if (obj.type == ValueType::Dict) {
        auto dictValue = std::get<std::shared_ptr<DictValue>>(obj.data);
        dictValue->items[name] = value;
        return;
    }
    runtimeError("STORE_ATTR unsupported for " + obj.repr());
}

void VM::opGetIter() {
    Value iterable = pop();
    std::vector<Value> values;
    if (iterable.type == ValueType::Iterator) {
        push(iterable);
        return;
    }
    if (iterable.type == ValueType::List) {
        values = std::get<std::shared_ptr<ListValue>>(iterable.data)->items;
    } else if (iterable.type == ValueType::String) {
        for (char ch : std::get<std::string>(iterable.data)) {
            values.push_back(Value::string(std::string(1, ch)));
        }
    } else if (iterable.type == ValueType::Dict) {
        for (const auto& item : std::get<std::shared_ptr<DictValue>>(iterable.data)->items) {
            values.push_back(Value::string(item.first));
        }
    } else {
        runtimeError("GET_ITER unsupported for " + iterable.repr());
    }
    push(Value::iterator(std::move(values)));
}

void VM::opForIter(const Instruction& inst, int ip, int& nextIp) {
    Value iterator = peek();
    if (iterator.type != ValueType::Iterator) {
        runtimeError("FOR_ITER expected iterator, got " + iterator.repr());
    }
    auto iteratorValue = std::get<std::shared_ptr<IteratorValue>>(iterator.data);
    if (iteratorValue->index >= iteratorValue->items.size()) {
        (void)pop();
        nextIp = jumpTarget(inst, ip);
        return;
    }
    push(iteratorValue->items[iteratorValue->index++]);
}

void VM::opSlice() {
    Value endValue = pop();
    Value startValue = pop();
    Value container = pop();
    if (container.type == ValueType::List) {
        const auto& items = std::get<std::shared_ptr<ListValue>>(container.data)->items;
        int64_t start = normalizeSliceIndex(startValue.asInt(), items.size());
        int64_t boundedEnd = normalizeSliceIndex(endValue.asInt(), items.size());
        if (boundedEnd < start) {
            boundedEnd = start;
        }
        std::vector<Value> out;
        for (int64_t i = start; i < boundedEnd; ++i) {
            out.push_back(items[static_cast<size_t>(i)]);
        }
        push(Value::list(std::move(out)));
        return;
    }
    if (container.type == ValueType::String) {
        const auto& text = std::get<std::string>(container.data);
        int64_t start = normalizeSliceIndex(startValue.asInt(), text.size());
        int64_t boundedEnd = normalizeSliceIndex(endValue.asInt(), text.size());
        if (boundedEnd < start) {
            boundedEnd = start;
        }
        push(Value::string(text.substr(static_cast<size_t>(start), static_cast<size_t>(boundedEnd - start))));
        return;
    }
    runtimeError("SLICE unsupported for " + container.repr());
}

void VM::opUnpackList(int count) {
    Value value = pop();
    std::vector<Value> items;
    if (value.type == ValueType::List) {
        items = std::get<std::shared_ptr<ListValue>>(value.data)->items;
    } else if (value.type == ValueType::Iterator) {
        auto iteratorValue = std::get<std::shared_ptr<IteratorValue>>(value.data);
        for (; iteratorValue->index < iteratorValue->items.size(); ++iteratorValue->index) {
            items.push_back(iteratorValue->items[iteratorValue->index]);
        }
    } else {
        runtimeError("UNPACK_LIST expected list/iterator, got " + value.repr());
    }
    if (count >= 0 && static_cast<size_t>(count) != items.size()) {
        runtimeError("UNPACK_LIST expected " + std::to_string(count) + " values, got " + std::to_string(items.size()));
    }
    for (const auto& item : items) {
        push(item);
    }
}

void VM::opUnpackDict(int count) {
    Value value = pop();
    if (value.type != ValueType::Dict) {
        runtimeError("UNPACK_DICT expected dict, got " + value.repr());
    }
    const auto& items = std::get<std::shared_ptr<DictValue>>(value.data)->items;
    if (count >= 0 && static_cast<size_t>(count) != items.size()) {
        runtimeError("UNPACK_DICT expected " + std::to_string(count) + " values, got " + std::to_string(items.size()));
    }
    for (const auto& item : items) {
        push(Value::string(item.first));
        push(item.second);
    }
}

void VM::opUnletAttr(const std::string& name) {
    Value obj = pop();
    if (obj.type == ValueType::Instance) {
        std::get<std::shared_ptr<InstanceValue>>(obj.data)->attrs.erase(name);
        return;
    }
    if (obj.type == ValueType::ClassType) {
        std::get<std::shared_ptr<ClassValue>>(obj.data)->attrs.erase(name);
        return;
    }
    if (obj.type == ValueType::Module) {
        std::get<std::shared_ptr<ModuleValue>>(obj.data)->attrs.erase(name);
        return;
    }
    if (obj.type == ValueType::Dict) {
        std::get<std::shared_ptr<DictValue>>(obj.data)->items.erase(name);
        return;
    }
    runtimeError("UNLET_ATTR unsupported for " + obj.repr());
}

void VM::opUnletSubscr() {
    Value index = pop();
    Value container = pop();
    if (container.type == ValueType::Dict) {
        std::get<std::shared_ptr<DictValue>>(container.data)->items.erase(index.dictKey());
        return;
    }
    if (container.type == ValueType::List) {
        auto items = std::get<std::shared_ptr<ListValue>>(container.data);
        int64_t idx = normalizeIndex(index.asInt(), items->items.size());
        if (idx < 0 || static_cast<size_t>(idx) >= items->items.size()) {
            runtimeError("list index out of range: " + std::to_string(idx));
        }
        items->items.erase(items->items.begin() + idx);
        return;
    }
    runtimeError("UNLET_SUBSCR unsupported for " + container.repr());
}

void VM::opLoadVariable(const Instruction& inst) {
    const std::string name = optionalNameAt(inst, "__var_" + std::to_string(inst.oparg));
    ScopedName scoped = splitScopedName(name);
    if (!scoped.scope.empty()) {
        const std::string& scope = scoped.scope;
        const std::string& actual = scoped.name;
        if (scopeMatches(scope, {"local", "name", "l"})) {
            auto found = currentFrame().locals.find(actual);
            if (found != currentFrame().locals.end()) {
                push(found->second);
                return;
            }
        } else if (scopeMatches(scope, {"global", "span", "g"})) {
            auto found = globals_.find(actual);
            if (found != globals_.end()) {
                push(found->second);
                return;
            }
        } else if (scopeMatches(scope, {"const", "c"})) {
            auto local = currentFrame().constVars.find(actual);
            if (local != currentFrame().constVars.end()) {
                push(local->second);
                return;
            }
            auto global = constGlobals_.find(actual);
            if (global != constGlobals_.end()) {
                push(global->second);
                return;
            }
        } else if (scopeMatches(scope, {"env", "e"})) {
            auto local = currentFrame().envVars.find(actual);
            if (local != currentFrame().envVars.end()) {
                push(local->second);
                return;
            }
            auto global = envGlobals_.find(actual);
            if (global != envGlobals_.end()) {
                push(global->second);
                return;
            }
        } else if (scopeMatches(scope, {"type"})) {
            auto local = currentFrame().typeVars.find(actual);
            if (local != currentFrame().typeVars.end()) {
                push(local->second);
                return;
            }
            auto global = typeGlobals_.find(actual);
            if (global != typeGlobals_.end()) {
                push(global->second);
                return;
            }
        } else if (scopeMatches(scope, {"script", "s"})) {
            auto local = currentFrame().scripts.find(actual);
            if (local != currentFrame().scripts.end()) {
                push(local->second);
                return;
            }
            auto global = scripts_.find(actual);
            if (global != scripts_.end()) {
                push(global->second);
                return;
            }
        } else if (scopeMatches(scope, {"module", "m"})) {
            push(getOrCreateModule(actual));
            return;
        }
        runtimeError("variable not found: " + name);
    }

    auto local = currentFrame().locals.find(name);
    if (local != currentFrame().locals.end()) {
        push(local->second);
        return;
    }
    auto constLocal = currentFrame().constVars.find(name);
    if (constLocal != currentFrame().constVars.end()) {
        push(constLocal->second);
        return;
    }
    auto envLocal = currentFrame().envVars.find(name);
    if (envLocal != currentFrame().envVars.end()) {
        push(envLocal->second);
        return;
    }
    auto typeLocal = currentFrame().typeVars.find(name);
    if (typeLocal != currentFrame().typeVars.end()) {
        push(typeLocal->second);
        return;
    }
    auto scriptLocal = currentFrame().scripts.find(name);
    if (scriptLocal != currentFrame().scripts.end()) {
        push(scriptLocal->second);
        return;
    }
    auto global = globals_.find(name);
    if (global != globals_.end()) {
        push(global->second);
        return;
    }
    auto constGlobal = constGlobals_.find(name);
    if (constGlobal != constGlobals_.end()) {
        push(constGlobal->second);
        return;
    }
    auto envGlobal = envGlobals_.find(name);
    if (envGlobal != envGlobals_.end()) {
        push(envGlobal->second);
        return;
    }
    auto typeGlobal = typeGlobals_.find(name);
    if (typeGlobal != typeGlobals_.end()) {
        push(typeGlobal->second);
        return;
    }
    auto scriptGlobal = scripts_.find(name);
    if (scriptGlobal != scripts_.end()) {
        push(scriptGlobal->second);
        return;
    }
    auto module = modules_.find(name);
    if (module != modules_.end()) {
        push(module->second);
        return;
    }
    runtimeError("variable not found: " + name);
}

void VM::opStoreVariable(const Instruction& inst, Value value) {
    const std::string name = optionalNameAt(inst, "__var_" + std::to_string(inst.oparg));
    ScopedName scoped = splitScopedName(name);
    if (!scoped.scope.empty()) {
        const std::string& scope = scoped.scope;
        const std::string& actual = scoped.name;
        if (scopeMatches(scope, {"local", "name", "l"})) {
            currentFrame().locals[actual] = value;
        } else if (scopeMatches(scope, {"global", "span", "g"})) {
            globals_[actual] = value;
        } else if (scopeMatches(scope, {"const", "c"})) {
            currentFrame().constVars[actual] = value;
            constGlobals_[actual] = value;
        } else if (scopeMatches(scope, {"env", "e"})) {
            currentFrame().envVars[actual] = value;
            envGlobals_[actual] = value;
        } else if (scopeMatches(scope, {"type"})) {
            currentFrame().typeVars[actual] = value;
            typeGlobals_[actual] = value;
        } else if (scopeMatches(scope, {"script", "s"})) {
            currentFrame().scripts[actual] = value;
            scripts_[actual] = value;
        } else if (scopeMatches(scope, {"module", "m"})) {
            modules_[actual] = value.type == ValueType::Module ? value : getOrCreateModule(actual);
        } else {
            runtimeError("unknown variable scope: " + scope);
        }
        return;
    }

    currentFrame().locals[name] = value;
    if (inst.opname == "STORE_VARIABLE_CONST") {
        currentFrame().constVars[name] = value;
        constGlobals_[name] = value;
    }
}

void VM::opTypeCast(const Instruction& inst) {
    Value value = pop();
    std::string target;
    if (value.type == ValueType::String && isCastTargetName(value.toString()) && hasStack(1)) {
        target = value.toString();
        value = pop();
    } else if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->constants.size()) {
        target = currentFrame().codeblock->constants[static_cast<size_t>(inst.oparg)].toString();
    } else {
        target = optionalNameAt(inst, std::to_string(inst.oparg));
    }
    if (target == "1" || target == "int" || target == "Int") {
        push(Value::integer(value.asInt()));
    } else if (target == "2" || target == "double" || target == "float" || target == "number") {
        push(Value::number(value.asDouble()));
    } else if (target == "3" || target == "string" || target == "String") {
        push(Value::string(value.toString()));
    } else if (target == "4" || target == "bool" || target == "Bool" || target == "boolean") {
        push(Value::boolean(value.truthy()));
    } else {
        push(value);
    }
}

void VM::opThrow(const Instruction& inst, int ip, int& nextIp) {
    Value value = popOrNull();
    pendingError_ = value.type == ValueType::Error ? value : Value::error(optionalNameAt(inst, "Thrown"), value);
    const auto& table = currentFrame().codeblock->exceptiontable;
    for (const auto& item : table) {
        if (ip >= item.startIp && ip <= item.endIp) {
            nextIp = item.handlerIp;
            return;
        }
    }
    runtimeError("uncaught throw: " + pendingError_.repr());
}

void VM::opExecNetaBlock(const Instruction& inst) {
    if (inst.oparg >= 0 && static_cast<size_t>(inst.oparg) < currentFrame().codeblock->blocks.size()) {
        push(executeBlock(blockAt(inst), {}));
        return;
    }
    Value value = popOrNull();
    push(makeMetadataObject("neta_result", {{"value", value}}));
}

void VM::opBuildGenerics(int count) {
    if (count < 0) {
        runtimeError("BUILD_GENERICS count cannot be negative");
    }
    std::vector<Value> values;
    for (int i = 0; i < count; ++i) {
        values.push_back(pop());
    }
    std::reverse(values.begin(), values.end());
    push(makeMetadataObject("generics", {{"items", Value::list(std::move(values))}}));
}

void VM::opBuildRecord(int count) {
    if (count < 0) {
        runtimeError("BUILD_RECORD count cannot be negative");
    }
    std::unordered_map<std::string, Value> values;
    for (int i = 0; i < count; ++i) {
        Value value = pop();
        Value key = pop();
        values[key.dictKey()] = value;
    }
    push(makeMetadataObject("record", {{"fields", Value::dict(std::move(values))}}));
}

void VM::opGenericsInstance() {
    Value generics = pop();
    Value value = pop();
    push(makeMetadataObject("generics_instance", {{"base", value}, {"generics", generics}}));
}

void VM::opLoadImplicitVariable(const Instruction& inst) {
    std::string name = optionalNameAt(inst, "__implicit_" + std::to_string(inst.oparg));
    auto local = currentFrame().locals.find(name);
    if (local != currentFrame().locals.end()) {
        push(local->second);
        return;
    }
    auto global = globals_.find(name);
    if (global != globals_.end()) {
        push(global->second);
        return;
    }
    push(Value::null());
}

Value VM::makeMetadataObject(const std::string& kind, std::unordered_map<std::string, Value> fields) const {
    fields["__kind"] = Value::string(kind);
    return Value::dict(std::move(fields));
}

std::string VM::fqnToPath(const std::string& fqn) const {
    std::string out = fqn;
    std::replace(out.begin(), out.end(), '.', '/');
    return out;
}

int VM::jumpTarget(const Instruction& inst, int ip) const {
    if (inst.resolvedTarget >= 0) {
        return inst.resolvedTarget;
    }
    if (inst.opname == "JUMP_FORWARD") {
        return ip + inst.oparg;
    }
    if (inst.opname == "JUMP_BACKWARD") {
        return ip - inst.oparg;
    }
    return inst.oparg;
}

std::shared_ptr<CodeBlock> VM::findCodeBlock(const std::shared_ptr<CodeBlock>& block, const std::string& name) const {
    if (!block) {
        return nullptr;
    }
    if (block->cbname == name) {
        return block;
    }
    for (const auto& child : block->blocks) {
        auto found = findCodeBlock(child, name);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

void VM::traceInstruction(const Instruction& inst, int ip, const std::vector<Value>& beforeStack) const {
    const Frame& frame = currentFrame();
    OpargExplanation explanation = explainOparg(*frame.codeblock, inst, registry_);
    std::cout << "[frame=" << frame.frameName << " ip=" << ip << "] "
              << inst.opname << " " << inst.oparg;
    if (!explanation.summary.empty()) {
        std::cout << " ; " << explanation.summary;
    }
    std::cout << "\n";
    std::cout << "oparg kind: " << opargKindName(explanation.kind) << "\n";
    if (!explanation.lookup.empty()) {
        std::cout << "lookup: " << explanation.lookup << "\n";
    }
    if (!explanation.result.empty()) {
        std::cout << "lookup result: " << explanation.result << "\n";
    }
    std::cout << "before stack: " << stackRepr(beforeStack) << "\n";
    std::cout << "after stack: " << stackRepr(frame.stack) << "\n";
    std::cout << "locals: " << mapRepr(frame.locals) << "\n";
    if (!globals_.empty()) {
        std::cout << "globals: " << mapRepr(globals_) << "\n";
    }
    std::cout << "\n";
}

void VM::runtimeError(const std::string& reason) const {
    std::ostringstream oss;
    oss << "RuntimeError:\n";
    if (!frames_.empty()) {
        const Frame& frame = currentFrame();
        oss << "  frame: " << frame.frameName << "\n";
        oss << "  ip: " << activeIp_ << "\n";
        if (activeInst_) {
            OpargExplanation explanation = explainOparg(*frame.codeblock, *activeInst_, registry_);
            oss << "  instruction: " << activeInst_->opname << " " << activeInst_->oparg;
            if (!explanation.summary.empty()) {
                oss << " ; " << explanation.summary;
            }
            oss << "\n";
            oss << "  oparg kind: " << opargKindName(explanation.kind) << "\n";
            if (!explanation.lookup.empty()) {
                oss << "  lookup: " << explanation.lookup << "\n";
            }
            if (!explanation.result.empty()) {
                oss << "  lookup result: " << explanation.result << "\n";
            }
        }
        oss << "  stack before: " << stackRepr(activeStackBefore_) << "\n";
        oss << "  stack current: " << stackRepr(frame.stack) << "\n";
        oss << "  locals: " << mapRepr(frame.locals) << "\n";
        if (!globals_.empty()) {
            oss << "  globals: " << mapRepr(globals_) << "\n";
        }
    }
    oss << "  reason: " << reason;
    throw RuntimeError(oss.str());
}

} // namespace stt
