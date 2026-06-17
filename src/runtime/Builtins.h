#pragma once

#include "vm/Value.h"

#include <string>
#include <unordered_map>

namespace rhino {

using BuiltinMap = std::unordered_map<std::string, Value>;

BuiltinMap createBuiltins();

} // namespace rhino
