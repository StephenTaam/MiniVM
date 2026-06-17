# Rhino Codegen Metadata VM Lab

A small training VM for Rhino/Phoenix codegen metadata. The VM does **not** parse Phoenix source; it loads a JSON metadata file containing `Program`, `CodeBlock`, static tables, and bytecode-like instructions, then can dump, disassemble via oparg explanations, trace, and execute the supported instruction subset.

## Build

```bash
make
```

The implementation is C++17 and has no third-party dependency; a compact JSON reader is embedded for the lab input format.

## Run

```bash
./rhino_lab run examples/minimal.json --trace --dump-tables
```

Options:

* `--dump-tables` prints constants, names, child blocks, instructions, and exception table placeholders.
* `--trace` prints every executed instruction with oparg kind, table lookup, before stack, and after stack.
* `--entry NAME` is accepted for CLI compatibility with the lab design.

## Supported behavior

Implemented core data structures and loader fields:

* `Program`, `CodeBlock`, `Instruction`, `ExceptionTableItem`, `Frame`, dynamic `Value`.
* Per-CodeBlock `constants`, `names`, `blocks`, `instructions`, `exceptiontable`, `instlnums`, function extras such as `argnames`.
* JSON instructions using either `{ "op": "LOAD_CONST", "arg": 0 }` or `{ "opcode": 52, "arg": 0 }`.
* Opcode registry with the documented opcode names so unknown-but-registered instructions can be loaded and reported clearly.

Implemented execution subset:

* Basic execution: `LOAD_CONST`, `LOAD_IMM8`, `LOAD_NONE`, `LOAD_NAME`, `STORE_NAME`, `LOAD_GLOBAL`, `STORE_GLOBAL`, `LOAD_VAR`, `STORE_VAR`, `LOAD_GVAR`, `STORE_GVAR`, `BINARY_OP`, `COMPARE_OP`, `UNARY_OP`, `TO_BOOL`, `POP_TOP`, `RHINO_ECHO`, `RETURN_VALUE`.
* Jumps: `JUMP_FORWARD`, `JUMP_BACKWARD`, `POP_JUMP_IF_FALSE`, `POP_JUMP_IF_TRUE`, `JUMP_IF_FALSE_OR_POP`, `JUMP_IF_TRUE_OR_POP`, including optional absolute `target` support.
* Functions: `DEFINE_FUN`/related aliases, `CALL`, `RETURN_VALUE`, argument binding through `argnames`, and `LOAD_ARG`.
* Containers: `BUILD_LIST`, `BUILD_DICT`, `LOAD_SUBSCR`, `STORE_SUBSCR`.

Unimplemented opcodes raise a runtime error that includes frame name, current ip, reason, and stack contents.

## Examples

* `examples/minimal.json`: computes `a = 1`, `b = 2`, `c = a + b`, echoes `3`.
* `examples/function.json`: defines and calls `add(a, b)`.
* `examples/loop.json`: demonstrates conditional jump and backward jump.
* `examples/containers.json`: builds a list and reads by subscript.
