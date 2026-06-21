cmake_minimum_required(VERSION 3.16)

project(stt_vm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(stt_vm
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

target_include_directories(stt_vm PRIVATE src)

if(MSVC)
    target_compile_options(stt_vm PRIVATE /W4)
else()
    target_compile_options(stt_vm PRIVATE -Wall -Wextra -pedantic)
endif()
