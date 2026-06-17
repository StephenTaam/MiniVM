# Rhino Codegen Metadata VM Lab

This is a small C++17 training VM for Rhino/Phoenix-style codegen metadata. It loads a JSON metadata file, resolves opcode/oparg pairs against each `CodeBlock` table, dumps disassembly, and executes a focused stack VM with trace output.

## Build

```bash
make
```

## Run

```bash
build/rhino_lab run examples/minimal.json --trace --dump-tables
```

For Windows or cross-platform builds, use CMake:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
```

中文使用说明见 [docs/USAGE.md](docs/USAGE.md)。

Supported commands:

```bash
build/rhino_lab run <metadata.json> [--trace] [--dump-tables] [--entry name]
build/rhino_lab dump <metadata.json>
build/rhino_lab disasm <metadata.json>
```

The first version intentionally focuses on the training surface:

- metadata JSON loading
- `constants` / `names` / `blocks` / `instructions` dumps
- opcode registry and oparg resolver
- stack-frame execution
- trace output before and after each instruction
- clear runtime errors for unsupported opcodes

No external JSON dependency is required; the project includes a small JSON parser in `src/util`.
