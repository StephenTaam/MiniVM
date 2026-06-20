# 语言和 VM 使用指南

这份文档回答一个核心问题：现在这个项目到底怎么“写程序”、怎么运行。

当前版本没有实现 `.ose` / Phoenix 源码解析器，也没有从源码生成 JSON 的编译器。可执行输入是静态 CodeBlock metadata JSON，加上一组栈式 bytecode 指令。可以把它理解成一门“metadata assembly”：

```text
源语言伪代码 -> 手写/生成 CodeBlock JSON -> rhino_lab VM 执行
```

以后如果补编译器，编译器只需要输出同一套 JSON 数据结构即可。

## 设计边界

用户资料里的数据结构是规范来源，本项目按这个结构读取和展示：

| CodeBlock 类 | `type` | 用途 |
| --- | ---: | --- |
| `CodeBlock` | `0` | 顶层 Script 代码块 |
| `FuncCodeBlock` | `1` | 函数代码块 |
| `ClassCodeBlock` | `2` | 类代码块 |
| `NetaCodeBlock` | `4` | Neta 代码块 |

指令名和 opcode 数值尽量按资料截图登记。能确定语义的指令会执行；语义不完整或当前训练 VM 不需要的指令，会保留在指令表里，支持 `dump` / `disasm`，执行到时抛出清晰的 `RuntimeError`。

## 当前语言形态

当前“语言”由三部分组成：

```text
1. Program 外壳：version / entry / root
2. CodeBlock 静态结构：constants / names / blocks / instructions 等
3. 栈式指令：LOAD_CONST、STORE_NAME、BINARY_OP、CALL、RETURN_VALUE 等
```

一段伪代码：

```text
a = 1
b = 2
echo a + b
```

对应的执行思路：

```text
1. constants 放字面量：1、2
2. names 放变量名：a、b
3. instructions 写入栈式操作：
   LOAD_CONST 0
   STORE_NAME 0
   LOAD_CONST 1
   STORE_NAME 1
   LOAD_NAME 0
   LOAD_NAME 1
   BINARY_OP 1
   ECHO 0
   RETURN_VALUE 0
```

## JSON 程序骨架

推荐用资料风格的数字 `type`：

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 1,
    "cbname": "main",
    "constants": [],
    "names": [],
    "blocks": [],
    "instructions": [],
    "exceptiontable": [],
    "instlnums": [],
    "baseline": 0,
    "argnames": [],
    "defaults": [],
    "flags": [],
    "annotations": [],
    "generics": []
  }
}
```

字段含义完整说明见 [METADATA_JSON.md](METADATA_JSON.md)。这里强调几个写程序时最常用的字段：

| 字段 | 作用 | 常见指令引用 |
| --- | --- | --- |
| `constants` | 常量池，放数字、字符串、列表、字典等字面量 | `LOAD_CONST arg` |
| `names` / `name` | 名字表，放变量名、函数名、内置函数名 | `LOAD_NAME`、`STORE_NAME`、`LOAD_BUILTIN_FUNC` |
| `blocks` | 子 CodeBlock 表，放函数、类、neta 子块 | `DEFINE_FUN`、`DEFINE_CLASS` |
| `instructions` | 指令序列，VM 按 ip 从前到后执行 | 所有 opcode |
| `argnames` | 函数参数名 | `CALL` 时绑定到函数 frame |
| `defaults` | 函数默认参数 | `CALL` 参数不足时补默认值 |

## 指令写法

推荐写字符串 opcode：

```json
{ "op": "LOAD_CONST", "oparg": 0 }
```

也可以写资料风格的数字 opcode：

```json
{ "opcode": 25, "oparg": 0 }
```

JSON 标准不支持 `0x19` 这种十六进制字面量，所以 `0x19 LOAD_CONST` 在 JSON 里写十进制 `25`。

`arg` 和 `oparg` 都支持。为了和资料一致，推荐写 `oparg`：

```json
{ "op": "STORE_NAME", "oparg": 0 }
```

## oparg 查表规则

同一个 `oparg: 0` 会因为 opcode 不同而指向不同表：

| 指令 | `oparg` 含义 |
| --- | --- |
| `LOAD_CONST 0` | `constants[0]` |
| `LOAD_NAME 0` | `names[0]` |
| `STORE_NAME 0` | `names[0]` |
| `DEFINE_FUN 0` | `blocks[0]` |
| `DEFINE_CLASS 0` | `blocks[0]` |
| `BINARY_OP 1` | `binaryop_t.NB_ADD` |
| `COMPARE_OP 5` | `compareop_t.CMP_GT` |
| `CALL 2` | 调用 2 个参数 |
| `JUMP_FORWARD 3` | 从当前 ip 向后跳 3 |

用 `--dump-tables` 或 `dump` 可以检查每条指令到底查到了哪一项。

## 栈式执行模型

VM 是栈式执行：

```text
LOAD_CONST 0     push constants[0]
STORE_NAME 0     pop 栈顶，保存到 locals[names[0]]
LOAD_NAME 0      push locals/globals/builtins[names[0]]
BINARY_OP 1      pop 右值、pop 左值，计算左值 + 右值，再 push 结果
ECHO 0           pop 栈顶并输出
RETURN_VALUE 0   pop 栈顶作为返回值；空栈则返回 null
```

函数调用的栈形状：

```text
[..., function, arg1, arg2]
CALL 2
```

调用时会创建新的 frame，并按 `argnames` 把参数绑定到函数局部变量。

## 推荐写程序流程

1. 先写伪代码，明确变量、常量、函数和类。
2. 把字面量放入当前 CodeBlock 的 `constants`。
3. 把变量名、函数名、内置函数名放入 `names`。
4. 把函数/类/neta 子块放入 `blocks`。
5. 手写 `instructions`，用 `oparg` 引用表下标。
6. 运行 `dump` 检查静态结构。
7. 运行 `run --trace --dump-tables` 检查栈变化。

```bash
build/rhino_lab dump your.json
build/rhino_lab run your.json --dump-tables --trace
```

## 示例：加法

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 1,
    "cbname": "main",
    "constants": [1, 2],
    "names": ["a", "b"],
    "blocks": [],
    "instructions": [
      { "op": "LOAD_CONST", "oparg": 0 },
      { "op": "STORE_NAME", "oparg": 0 },
      { "op": "LOAD_CONST", "oparg": 1 },
      { "op": "STORE_NAME", "oparg": 1 },
      { "op": "LOAD_NAME", "oparg": 0 },
      { "op": "LOAD_NAME", "oparg": 1 },
      { "op": "BINARY_OP", "oparg": 1 },
      { "op": "ECHO", "oparg": 0 },
      { "op": "RETURN_VALUE", "oparg": 0 }
    ],
    "exceptiontable": [],
    "instlnums": [],
    "baseline": 0
  }
}
```

`BINARY_OP 1` 是 `NB_ADD`。运行后输出 `3`。

## 示例：函数

伪代码：

```text
fun add(a, b) {
  return a + b
}

echo add(1, 2)
```

关键结构：

```text
root.blocks[0]          -> add 的 FuncCodeBlock
root.instructions:
  DEFINE_FUN 0          -> 把 blocks[0] 做成函数对象
  STORE_NAME 0          -> 保存为 names[0]，例如 "add"
  LOAD_NAME 0           -> 取函数对象
  LOAD_CONST 0/1        -> 压入实参
  CALL 2                -> 调用两个参数
  ECHO 0                -> 输出返回值
```

完整文件见 `examples/function_call.json`。

## 示例：资料静态结构

`examples/codeblock_static_shape.json` 用来验证截图里的结构：

```bash
build/rhino_lab dump examples/codeblock_static_shape.json
```

它包含：

```text
root type=0 CodeBlock
blocks[0] type=2 ClassCodeBlock
ClassCodeBlock.inherits
ClassCodeBlock.annotations
ClassCodeBlock.syntactic
ClassCodeBlock.generics
```

其中 `DECLARE_PACKAGE`、`STORE_CONST_VAR`、`LOAD_IMPLICIT_VARIABLE` 等目前只注册和反汇编，不作为可执行核心语义。

## 当前实现策略

和资料保持一致的部分：

```text
1. CodeBlock type 数字：0/1/2/4
2. 静态字段：cbname/constants/names/blocks/instructions/exceptiontable/instlnums/baseline 等
3. 函数扩展字段：argnames/defaults/flags/annotations/generics
4. 类扩展字段：inherits/annotations/syntactic/generics
5. Neta 扩展字段：annotations
6. opcode 名称和已知 opcode 数值
```

训练 VM 自己决定的部分：

```text
1. 读取 JSON，不读取二进制 RSE 文件
2. 子 CodeBlock 统一显式放在 blocks，而不是在 constants 中递归展开
3. `currline` 是训练版保留字段，资料核心表里没有，当前只保存和 dump
4. 字符串 type 只作为便利兼容，规范写法仍是数字 type
5. 未实现 opcode 保留在 registry，执行到时报 RuntimeError
```

## 目前不包含什么

当前没有：

```text
1. `.ose` 源码 parser
2. 从源码自动生成 CodeBlock JSON 的 compiler
3. 二进制 RSE 读写
4. Harmony/Neta 真实运行时
5. 完整异常、生成器、泛型、模块导入、环境变量语义
```

这些都可以在后续迭代中基于同一套 CodeBlock 数据结构继续补。
