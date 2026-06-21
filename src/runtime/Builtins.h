#pragma once

#include "vm/Value.h"

#include <string>
#include <unordered_map>

namespace stt {

using BuiltinMap = std::unordered_map<std::string, Value>;

BuiltinMap createBuiltins();

} // namespace stt
