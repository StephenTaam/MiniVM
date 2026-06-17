#pragma once

#include "metadata/Program.h"
#include "runtime/Builtins.h"
#include "vm/Frame.h"
#include "vm/Opcode.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace rhino {

class RuntimeError : public std::runtime_error {
public:
    explicit RuntimeError(const std::string& message) : std::runtime_error(message) {}
};

class VM {
public:
    VM(Program program, const OpcodeRegistry& registry);

    bool trace = false;
    bool dumpTables = false;

    Value run(const std::string& entryOverride = "");

private:
    Program program_;
    const OpcodeRegistry& registry_;
    std::vector<Frame> frames_;
    std::unordered_map<std::string, Value> globals_;
    BuiltinMap builtins_;

    const Instruction* activeInst_ = nullptr;
    int activeIp_ = -1;
    std::vector<Value> activeStackBefore_;

    Value executeBlock(std::shared_ptr<CodeBlock> codeblock, const std::vector<Value>& args);
    void bindArgs(Frame& frame, const std::vector<Value>& args);
    void executeInstruction(const Instruction& inst, int ip, int& nextIp, bool& didReturn, Value& returnValue);

    Frame& currentFrame();
    const Frame& currentFrame() const;
    Value pop();
    Value peek() const;
    void push(Value value);

    Value resolveName(const std::string& name);
    Value resolveGlobal(const std::string& name);
    const std::string& nameAt(const Instruction& inst);
    Value constAt(const Instruction& inst);
    std::shared_ptr<CodeBlock> blockAt(const Instruction& inst);

    void opBinary(int op);
    void opUnary(int op);
    void opCompare(int op);
    void opCall(int argCount);
    void opLoadSubscr();
    void opStoreSubscr();
    void opLoadAttr(const std::string& name);
    void opStoreAttr(const std::string& name);

    int jumpTarget(const Instruction& inst, int ip) const;
    std::shared_ptr<CodeBlock> findCodeBlock(const std::shared_ptr<CodeBlock>& block, const std::string& name) const;

    void traceInstruction(const Instruction& inst, int ip, const std::vector<Value>& beforeStack) const;
    [[noreturn]] void runtimeError(const std::string& reason) const;
};

} // namespace rhino
