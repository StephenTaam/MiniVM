# STT Metadata VM

This is a small C++17 training VM for STT codegen metadata. It loads a JSON metadata file, resolves opcode/oparg pairs against each `CodeBlock` table, dumps disassembly, and executes a focused stack VM with trace output.

## Build

```bash
make
```

## Run

```bash
build/stt_vm run examples/minimal.json --trace --dump-tables
```

For Windows or cross-platform builds, use CMake:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
```

中文使用说明见 [docs/USAGE.md](docs/USAGE.md)。

当前语言/VM 的写法、执行模型和设计边界见 [docs/LANGUAGE_VM_GUIDE.md](docs/LANGUAGE_VM_GUIDE.md)。

指令集参考见 [docs/INSTRUCTION_SET.md](docs/INSTRUCTION_SET.md)。

Metadata JSON 字段说明见 [docs/METADATA_JSON.md](docs/METADATA_JSON.md)。

与原始资料对齐的 CodeBlock 字段和 opcode 数值表见 [docs/SOURCE_COMPAT.md](docs/SOURCE_COMPAT.md)。

Supported commands:

```bash
build/stt_vm run <metadata.json> [--trace] [--dump-tables] [--entry name]
build/stt_vm dump <metadata.json>
build/stt_vm disasm <metadata.json>
```

The first version intentionally focuses on the training surface:

- metadata JSON loading
- `constants` / `names` / `blocks` / `instructions` dumps
- opcode registry and oparg resolver
- stack-frame execution
- trace output before and after each instruction
- clear runtime errors for unknown or illegal opcodes

No external JSON dependency is required; the project includes a small JSON parser in `src/util`.
