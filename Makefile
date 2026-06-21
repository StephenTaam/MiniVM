CXX ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -O2
CPPFLAGS ?= -Isrc

BUILD_DIR := build
TARGET := $(BUILD_DIR)/stt_vm

SOURCES := \
	src/main.cpp \
	src/debug/Disassembler.cpp \
	src/metadata/Loader.cpp \
	src/metadata/Verifier.cpp \
	src/runtime/Builtins.cpp \
	src/util/Json.cpp \
	src/vm/Opcode.cpp \
	src/vm/VM.cpp \
	src/vm/Value.cpp

OBJECTS := $(SOURCES:%.cpp=$(BUILD_DIR)/%.o)

.PHONY: all clean run-example

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

run-example: $(TARGET)
	$(TARGET) run examples/minimal.json --dump-tables --trace

clean:
	rm -rf $(BUILD_DIR)
