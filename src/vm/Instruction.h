#pragma once

#include <string>

namespace stt {

struct Instruction {
    int opcode = 0;
    std::string opname;
    int oparg = 0;
    int line = -1;
    int resolvedTarget = -1;
};

struct ExceptionTableItem {
    int startIp = 0;
    int endIp = 0;
    int handlerIp = 0;
    std::string type;
};

} // namespace stt
