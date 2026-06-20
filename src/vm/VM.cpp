#include "vm/VM.h"

#include "debug/Disassembler.h"

#include <algorithm>
#include <iostream>
#include <sstream>
#include <unordered_set>

namespace rhino {
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

} // namespace

VM::VM(Program program, const OpcodeRegistry& registry)
    : program_(std::move(program)), registry_(registry), builtins_(createBuiltins()) {}

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

Value VM::executeBlock(std::shared_ptr<CodeBlock> codeblock, const std::vector<Value>& args) {
    Frame frame;
    frame.codeblock = std::move(codeblock);
    frame.ip = 0;
    frame.args = args;
    frame.frameName = frame.codeblock && !frame.codeblock->cbname.empty() ? frame.codeblock->cbname : "<anonymous>";
    bindArgs(frame, args);
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

void VM::bindArgs(Frame& frame, const std::vector<Value>& args) {
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
    for (size_t i = 0; i < argnames.size(); ++i) {
        if (i < args.size()) {
            frame.locals[argnames[i]] = args[i];
        } else if (i >= defaultsStart && (i - defaultsStart) < frame.codeblock->defaults.size()) {
            frame.locals[argnames[i]] = frame.codeblock->defaults[i - defaultsStart];
        } else {
            frame.locals[argnames[i]] = Value::null();
        }
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
    } else if (op == "LOAD_NAME") {
        push(resolveName(nameAt(inst)));
    } else if (op == "STORE_NAME" || op == "STORE_VAR") {
        currentFrame().locals[nameAt(inst)] = pop();
    } else if (op == "LOAD_GLOBAL" || op == "LOAD_GVAR") {
        push(resolveGlobal(nameAt(inst)));
    } else if (op == "STORE_GLOBAL" || op == "STORE_GVAR") {
        globals_[nameAt(inst)] = pop();
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
    } else if (isFunctionDefineOpcode(op)) {
        push(Value::function(blockAt(inst)));
    } else if (op == "DEFINE_CLASS") {
        push(Value::classType(blockAt(inst)));
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
    } else if (op == "CALL") {
        opCall(inst.oparg);
    } else if (op == "RETURN_VALUE") {
        returnValue = currentFrame().stack.empty() ? Value::null() : pop();
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
    } else if (op == "POP_TOP") {
        (void)pop();
    } else if (op == "ECHO" || op == "RHINO_ECHO") {
        std::cout << pop().toString() << "\n";
    } else if (op == "TO_BOOL") {
        push(Value::boolean(pop().truthy()));
    } else if (op == "JUMP_FORWARD" || op == "JUMP_BACKWARD") {
        nextIp = jumpTarget(inst, ip);
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

Value VM::resolveName(const std::string& name) {
    auto local = currentFrame().locals.find(name);
    if (local != currentFrame().locals.end()) {
        return local->second;
    }
    auto global = globals_.find(name);
    if (global != globals_.end()) {
        return global->second;
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
        case 9:
            push(Value::boolean(left.truthy() || right.truthy()));
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
    if (argCount < 0) {
        runtimeError("CALL arg count cannot be negative");
    }

    std::vector<Value> args;
    for (int i = 0; i < argCount; ++i) {
        args.push_back(pop());
    }
    std::reverse(args.begin(), args.end());

    Value callable = pop();
    if (callable.type == ValueType::Function) {
        auto fn = std::get<std::shared_ptr<FunctionValue>>(callable.data);
        if (!fn || !fn->codeblock) {
            runtimeError("attempted to call null function");
        }
        Value result = executeBlock(fn->codeblock, args);
        push(result);
        return;
    }
    if (callable.type == ValueType::BuiltinFunction) {
        auto builtin = std::get<std::shared_ptr<BuiltinValue>>(callable.data);
        Value result = builtin->fn(args);
        push(result);
        return;
    }

    runtimeError("CALL expected function, got " + callable.repr());
}

void VM::opLoadSubscr() {
    Value index = pop();
    Value container = pop();
    if (container.type == ValueType::List) {
        const auto& items = std::get<std::shared_ptr<ListValue>>(container.data)->items;
        int64_t idx = index.asInt();
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
        int64_t idx = index.asInt();
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
        int64_t idx = index.asInt();
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
        auto it = klass->attrs.find(name);
        if (it == klass->attrs.end()) {
            runtimeError("class attribute not found: " + name);
        }
        push(it->second);
        return;
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
    runtimeError("STORE_ATTR unsupported for " + obj.repr());
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

} // namespace rhino
