cmake_minimum_required(VERSION 3.16)

project(rhino_lab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(rhino_lab
    src/main.cpp
    src/debug/Disassembler.cpp
    src/metadata/Loader.cpp
    src/metadata/Verifier.cpp
    src/runtime/Builtins.cpp
    src/util/Json.cpp
    src/vm/Opcode.cpp
    src/vm/VM.cpp
    src/vm/Value.cpp
)

target_include_directories(rhino_lab PRIVATE src)

if(MSVC)
    target_compile_options(rhino_lab PRIVATE /W4)
else()
    target_compile_options(rhino_lab PRIVATE -Wall -Wextra -pedantic)
endif()
