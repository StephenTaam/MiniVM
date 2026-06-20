# Rhino Lab 使用说明

## 平台支持

代码本身只使用标准 C++17 和标准库，没有第三方依赖，也没有使用 macOS/Linux 专有 API。

| 平台 | 支持情况 | 推荐方式 |
| --- | --- | --- |
| macOS | 支持 | `make` 或 CMake |
| Linux | 支持 | `make` 或 CMake |
| Windows | 支持源码编译 | CMake + Visual Studio/MSVC、MinGW，或 WSL |

说明：

- 当前 `Makefile` 主要面向 macOS/Linux/WSL。
- Windows 原生环境建议用 CMake，因为 Visual Studio 不能直接使用这个 Unix 风格 Makefile。
- 程序读取的是 UTF-8 JSON 文件。Windows 终端如果显示中文路径或中文字符串异常，可以切换到 UTF-8 代码页。

## 构建

### macOS / Linux

```bash
make
```

生成程序：

```bash
build/rhino_lab
```

### Windows / macOS / Linux 通用 CMake

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
```

Windows Visual Studio 生成的可执行文件通常在：

```text
build-cmake\Debug\rhino_lab.exe
```

Release 构建：

```bash
cmake -S . -B build-cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake --config Release
```

Visual Studio 多配置生成器下，Release 可执行文件通常在：

```text
build-cmake\Release\rhino_lab.exe
```

## 命令格式

```bash
rhino_lab run <metadata.json> [--trace] [--dump-tables] [--entry name]
rhino_lab dump <metadata.json>
rhino_lab disasm <metadata.json>
```

### run

加载 metadata JSON，并执行 VM。

```bash
build/rhino_lab run examples/minimal.json
```

带 trace 和静态表 dump：

```bash
build/rhino_lab run examples/minimal.json --dump-tables --trace
```

指定入口 CodeBlock：

```bash
build/rhino_lab run examples/function_call.json --entry root --trace
```

### dump

只加载 metadata，并打印 Program / CodeBlock / constants / names / blocks / instructions / exceptiontable。
现在也会打印 CodeBlock 静态扩展字段，包括 `baseline`、`currline`、`instlnums`、`argnames`、`defaults`、`flags`、`inherits`、`annotations`、`syntactic`、`generics`。

```bash
build/rhino_lab dump examples/minimal.json
```

查看资料风格的 ClassCodeBlock 静态字段：

```bash
build/rhino_lab dump examples/codeblock_static_shape.json
```

### disasm

当前和 `dump` 输出一致，重点展示每条指令的 oparg 解释。

```bash
build/rhino_lab disasm examples/function_call.json
```

## 示例

### 最小示例

```bash
build/rhino_lab run examples/minimal.json --dump-tables --trace
```

当前 `examples/minimal.json` 表达的是：

```text
a = 3
b = 2
c = a * b
echo c
```

预期会在输出中看到：

```text
LOAD_CONST 0 ; constants[0] = int(3)
STORE_NAME 0 ; names[0] = a
BINARY_OP 3 ; NB_MULTIPLY
6
```

### 函数调用示例

```bash
build/rhino_lab run examples/function_call.json --dump-tables --trace
```

这个示例覆盖：

- `DEFINE_FUN`
- `CALL`
- `LOAD_ARG`
- 子 frame 执行
- `RETURN_VALUE` 返回到父 frame

## Metadata JSON 基本结构

当前没有源码 parser，正式输入是静态 CodeBlock JSON。如何把伪代码整理成 JSON、VM 如何执行，见 [LANGUAGE_VM_GUIDE.md](LANGUAGE_VM_GUIDE.md)。

完整字段说明见 [METADATA_JSON.md](METADATA_JSON.md)。

如果你是按资料截图里的 `type: 0`、`opcode/oparg`、`ECHO`、`LOAD_BUILTIN_FUN` 等风格写，可以看 [SOURCE_COMPAT.md](SOURCE_COMPAT.md)。项目里也有对应示例：

```bash
build/rhino_lab run examples/source_style.json --dump-tables --trace
```

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 0,
    "cbname": "main",
    "constants": [
      { "type": "int", "value": 1 },
      { "type": "int", "value": 2 }
    ],
    "names": ["a", "b", "c"],
    "blocks": [],
    "instructions": [
      { "opcode": 25, "oparg": 0 },
      { "opcode": 50, "oparg": 0 },
      { "op": "LOAD_CONST", "oparg": 1 },
      { "op": "STORE_NAME", "oparg": 1 },
      { "op": "LOAD_NAME", "oparg": 0 },
      { "op": "LOAD_NAME", "oparg": 1 },
      { "op": "BINARY_OP", "oparg": 1 },
      { "opcode": 50, "oparg": 2 },
      { "op": "LOAD_NAME", "oparg": 2 },
      { "op": "ECHO", "oparg": 0 },
      { "opcode": 44, "oparg": 0 }
    ],
    "exceptiontable": [],
    "instlnums": [],
    "baseline": 0
  }
}
```

## Trace 怎么看

每条指令会显示：

```text
[frame=main ip=6] BINARY_OP 1 ; NB_ADD
oparg kind: BinaryOp
lookup: binaryop_t(1)
lookup result: NB_ADD
before stack: [int(1), int(2)]
after stack: [int(3)]
locals: {b = int(2), a = int(1)}
```

重点看这几项：

- `oparg kind`：当前 oparg 被解释成什么类型。
- `lookup`：查的是哪张表或哪个枚举。
- `lookup result`：查表结果。
- `before stack` / `after stack`：栈式 VM 的核心状态变化。
- `locals` / `globals`：变量环境变化。

## 已实现的核心指令

第一版已实现：

```text
LOAD_CONST LOAD_IMM8 LOAD_NONE
LOAD_NAME STORE_NAME
LOAD_GLOBAL STORE_GLOBAL
LOAD_VAR STORE_VAR LOAD_GVAR STORE_GVAR LOAD_TVAR STORE_TVAR LOAD_BVAR STORE_BVAR
LOAD_ARG LOAD_BUILTIN_FUNC
BINARY_OP UNARY_OP COMPARE_OP
BUILD_LIST BUILD_DICT LOAD_SUBSCR STORE_SUBSCR
POP_TOP ECHO TO_BOOL
JUMP_FORWARD JUMP_BACKWARD
POP_JUMP_IF_FALSE POP_JUMP_IF_TRUE
JUMP_IF_FALSE_OR_POP JUMP_IF_TRUE_OR_POP
DEFINE_FUN DEFINE_FUNCTION DEFINE_GFUN DEFINE_TFUN DEFINE_PRELOADER DEFINE_CONSTRUCTOR DEFINE_LAMBDA DEFINE_CLOSURE DEFINE_ANONYMOUS
CALL RETURN_VALUE
DEFINE_CLASS CREATE_INSTANCE LOAD_ATTR STORE_ATTR
```

更详细的每条指令说明、栈变化和示例见 [INSTRUCTION_SET.md](INSTRUCTION_SET.md)。

未实现的 opcode 会抛出带上下文的 `RuntimeError`，里面包含：

- frame 名称
- ip
- instruction
- oparg 解释
- stack before/current
- locals/globals
- reason

## 清理构建产物

Makefile 构建：

```bash
make clean
```

CMake 构建：

```bash
rm -rf build-cmake
```
