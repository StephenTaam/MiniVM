#pragma once

#include "metadata/Program.h"
#include "runtime/Builtins.h"
#include "vm/Frame.h"
#include "vm/Opcode.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace stt {

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
    std::unordered_map<std::string, Value> constGlobals_;
    std::unordered_map<std::string, Value> envGlobals_;
    std::unordered_map<std::string, Value> typeGlobals_;
    std::unordered_map<std::string, Value> scripts_;
    std::unordered_map<std::string, Value> modules_;
    std::unordered_set<std::string> loadedModules_;
    Value pendingError_;
    BuiltinMap builtins_;

    const Instruction* activeInst_ = nullptr;
    int activeIp_ = -1;
    std::vector<Value> activeStackBefore_;

    Value executeBlock(
        std::shared_ptr<CodeBlock> codeblock,
        const std::vector<Value>& args,
        const std::unordered_map<std::string, Value>& namedArgs = {});
    void bindArgs(
        Frame& frame,
        const std::vector<Value>& args,
        const std::unordered_map<std::string, Value>& namedArgs);
    void executeInstruction(const Instruction& inst, int ip, int& nextIp, bool& didReturn, Value& returnValue);

    Frame& currentFrame();
    const Frame& currentFrame() const;
    Value pop();
    Value peek() const;
    void push(Value value);

    Value resolveName(const std::string& name);
    Value resolveGlobal(const std::string& name);
    const std::string& nameAt(const Instruction& inst);
    std::string optionalNameAt(const Instruction& inst, const std::string& fallback) const;
    Value constAt(const Instruction& inst);
    std::shared_ptr<CodeBlock> blockAt(const Instruction& inst);
    bool hasStack(size_t count) const;
    Value popOrNull();
    Value getOrCreateModule(const std::string& name);
    const Value* metadataField(const Value& value, const std::string& field) const;
    bool isMetadataKind(const Value& value, const std::string& kind) const;
    std::string metadataFieldString(const Value& value, const std::string& field, const std::string& fallback) const;
    std::vector<Value> collectCallArguments(int argCount, std::unordered_map<std::string, Value>& namedArgs);

    void opBinary(int op);
    void opUnary(int op);
    void opCompare(int op);
    void opCall(int argCount);
    void opLoadSubscr();
    void opStoreSubscr();
    void opLoadAttr(const std::string& name);
    void opStoreAttr(const std::string& name);
    void opGetIter();
    void opForIter(const Instruction& inst, int ip, int& nextIp);
    void opSlice();
    void opUnpackList(int count);
    void opUnpackDict(int count);
    void opUnletAttr(const std::string& name);
    void opUnletSubscr();
    void opLoadVariable(const Instruction& inst);
    void opStoreVariable(const Instruction& inst, Value value);
    void opTypeCast(const Instruction& inst);
    void opThrow(const Instruction& inst, int ip, int& nextIp);
    void opExecNetaBlock(const Instruction& inst);
    void opBuildGenerics(int count);
    void opBuildRecord(int count);
    void opGenericsInstance();
    void opLoadImplicitVariable(const Instruction& inst);
    Value makeMetadataObject(const std::string& kind, std::unordered_map<std::string, Value> fields) const;
    std::string fqnToPath(const std::string& fqn) const;

    int jumpTarget(const Instruction& inst, int ip) const;
    std::shared_ptr<CodeBlock> findCodeBlock(const std::shared_ptr<CodeBlock>& block, const std::string& name) const;

    void traceInstruction(const Instruction& inst, int ip, const std::vector<Value>& beforeStack) const;
    [[noreturn]] void runtimeError(const std::string& reason) const;
};

} // namespace stt
