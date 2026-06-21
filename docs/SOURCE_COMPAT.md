# 资料对照与兼容说明

这份文档专门记录与原始资料对齐后的约定，方便把文档里的 CodeBlock / Bytecode / Instruction Set 转成 `stt_vm` 可读 JSON。

当前没有源码 parser，正式写法和执行模型见 [LANGUAGE_VM_GUIDE.md](LANGUAGE_VM_GUIDE.md)。本文件只记录与原始资料的对照关系。

## 已发现并修正的出入

| 项 | 资料中的形式 | 原训练 VM 形式 | 当前处理 |
| --- | --- | --- | --- |
| CodeBlock `type` | 数字：`0/1/2/4` | 字符串：`normal/function/class/neta` | 数字是规范写法；内部枚举值也已对齐 `0/1/2/4`，仍兼容字符串 |
| 名字表字段 | 部分示例写 `name` | 文档和代码写 `names` | 已兼容 `name` 和 `names` |
| 指令参数字段 | `(opcode, oparg)` | JSON 示例用 `arg` | 已兼容 `arg` 和 `oparg` |
| opcode 数字 | 资料使用 `0x19 LOAD_CONST` 等 | 原 registry 是顺序编号 | 已按原始资料为已知 opcode 注册数值 |
| echo 指令 | 示例里写 `ECHO` | 训练 VM 使用 `ECHO` | `ECHO` 是规范名 |
| bool 转换 | `CONVERT_BOOL` | `TO_BOOL` | 已把 `CONVERT_BOOL` 作为别名 |
| builtin 名称 | `LOAD_BUILTIN_FUN` | `LOAD_BUILTIN_FUNC` | 已作为别名 |
| span/global 名称 | `LOAD_SPAN` / `STORE_SPAN` | `LOAD_GLOBAL` / `STORE_GLOBAL` | 已作为别名 |
| syntactic 空值 | 示例里可能是 `[]` | 原 loader 只收 object | 已允许空数组 |

## CodeBlock 层次结构

资料里 CodeBlock 是所有 bytecode 块的基类，子类针对不同语法结构扩展字段。

| 类名 | type | 用途 | 单个可执行文件中数量 |
| --- | --- | --- | --- |
| `CodeBlock` | `0` | Script 代码块，最大层脚本块 | 仅一个 |
| `FuncCodeBlock` | `1` | Function 代码块 | 多个 |
| `ClassCodeBlock` | `2` | Class 代码块 | 多个 |
| `NetaCodeBlock` | `4` | Neta 代码块 | 多个 |

资料里也说明：CodeBlock 类似 C 程序的 Text / Data / Bss 段。

本项目中的 `dump` 会同时显示数字和说明，例如：

```text
=== CodeBlock: script type=0 normal ===
  [0] type=2 class Example
```

## 静态字段矩阵

### 基础 CodeBlock type=0

| 字段 | 类型 | 功能描述 | 备注 |
| --- | --- | --- | --- |
| `type` | Number | 块类型 | `0` |
| `cbname` | String | 块名称 | 也用于 trace frame 名 |
| `constants` | List | 常量池 | `LOAD_CONST` 查这里 |
| `names` / `name` | List | 变量名、函数名等 | `LOAD_NAME` / `STORE_NAME` 查这里 |
| `blocks` | List | 嵌套子块 | `DEFINE_CLASS` / `DEFINE_FUN` 查这里 |
| `instructions` | List | `(opcode, oparg)` 指令序列 | JSON 中写 object 数组 |
| `exceptiontable` | List | 异常处理表 | 资料备注 Flaw；当前参与 `THROW` / `CATCH` 的训练版跳转 |
| `instlnums` | List | 指令对应源码行号 | 可为空 |
| `baseline` | Number | 起始行号偏移 | 当前只保存 |

### FuncCodeBlock type=1

在基础字段上增加：

| 字段 | 类型 | 功能描述 |
| --- | --- | --- |
| `argnames` | List | 参数列表 |
| `defaults` | List | 参数默认值 |
| `flags` | List | 函数类型 |
| `annotations` | List | 注解列表 |
| `generics` | List | 泛型列表 |

### ClassCodeBlock type=2

在基础字段上增加：

| 字段 | 类型 | 功能描述 |
| --- | --- | --- |
| `inherits` | List | 继承列表 |
| `annotations` | List | 注解列表 |
| `syntactic` | Dict | 扩展特性列表 |
| `generics` | List | 泛型列表 |

### NetaCodeBlock type=4

在基础字段上增加：

| 字段 | 类型 | 功能描述 |
| --- | --- | --- |
| `annotations` | List | 注解列表 |

## Bytecode 生成顺序

资料里的 Bytecode 生成流程：

```text
1. 写入指令集版本号
2. 中序遍历 CodeBlock 树
3. 依次写入每个 CodeBlock 的 Name / Constants / Names / Instructions 等属性
4. 写入 Constants 时如果遇到子 CodeBlock，则递归写入子 CodeBlock
```

训练 VM 当前读取 JSON，不读取二进制 RSE。为了训练 `oparg -> table` 的关系，JSON 中把子 CodeBlock 显式放到 `blocks`：

```text
DEFINE_CLASS 0 -> blocks[0]
DEFINE_FUN 1   -> blocks[1]
```

## 当前实现取舍

严格按资料保留的部分：

```text
1. CodeBlock 层次和值：CodeBlock=0、FuncCodeBlock=1、ClassCodeBlock=2、NetaCodeBlock=4
2. CodeBlock 静态字段：cbname、constants、names、blocks、instructions、exceptiontable、instlnums、baseline
3. 子类扩展字段：argnames、defaults、flags、inherits、annotations、syntactic、generics
4. 已知 opcode 数字和指令名
5. 资料中的别名：ECHO、LOAD_BUILTIN_FUN、STORE_CODEBLOCK、CONVERT_BOOL 等
```

训练 VM 做出的折中：

```text
1. 读取 JSON 而不是 RSE 二进制文件
2. 子 CodeBlock 统一放入 blocks，便于通过 oparg 查表
3. 字符串 type 只作为兼容写法，规范文档和示例优先使用数字 type
4. 依赖 Harmony/Neta 运行时的指令使用训练版本地语义，例如模块表、脚本表、metadata 对象和 bookkeeping
5. 执行到未知指令或非法指令会报 RuntimeError，并带 frame/ip/oparg/stack/locals 信息
```

## 资料风格 JSON 示例

示例文件：

```text
examples/source_style.json
examples/codeblock_static_shape.json
```

`source_style.json` 验证了这些兼容点：

```text
type: 0
opcode: 25       -> LOAD_CONST
opcode: 50       -> STORE_NAME
opcode: 44       -> RETURN_VALUE
oparg: 0
op: "ECHO"       -> ECHO
```

运行：

```bash
build/stt_vm run examples/source_style.json --dump-tables --trace
```

`codeblock_static_shape.json` 主要用于观察 ClassCodeBlock 静态字段：

```bash
build/stt_vm dump examples/codeblock_static_shape.json
```

## 已知 opcode 数值表

下面是根据原始资料录入的 opcode。未列出的训练扩展 opcode 仍可用字符串 `op`，但其数字编号使用训练 VM 内部保留区，不建议依赖。

| Opcode | 十进制 | Instruction | 资料描述 | 当前执行状态 |
| --- | ---: | --- | --- | --- |
| `0x02` | 2 | `BINARY_OP` | pop two variables and do binary operation | 已实现，除 `NB_UNKNOWN` |
| `0x03` | 3 | `BUILD_DICT` | pop values to build dict | 已实现 |
| `0x04` | 4 | `BUILD_LIST` | pop values to build list | 已实现 |
| `0x05` | 5 | `CALL` | pop args and call function | 已实现 |
| `0x06` | 6 | `COMPARE_OP` | pop two variables and compare | 已实现，除 `CMP_UNKNOWN` |
| `0x07` | 7 | `CREATE_INSTANCE` | create instance and push result | 已实现训练版 |
| `0x08` | 8 | `DECLARE_IMPORT` | declare import | 已实现训练版 |
| `0x09` | 9 | `DECLARE_PACKAGE` | declare package | 已实现训练版 |
| `0x0a` | 10 | `DEFINE_CLASS` | define class | 已实现训练版 |
| `0x0c` | 12 | `DEFINE_MODULE` | define module | 已实现训练版 |
| `0x0d` | 13 | `DEFINE_ANNOTATION` / `DEFINE_NOTE` | define annotation | 已实现训练版 |
| `0x0e` | 14 | `DEFINE_FLAW` | define flaw | 已实现训练版 |
| `0x10` | 16 | `FOR_ITER` | iterate next item | 已实现 |
| `0x12` | 18 | `GET_ITER` | get iterator | 已实现 |
| `0x13` | 19 | `JUMP_BACKWARD` | jump backward by oparg | 已实现 |
| `0x14` | 20 | `JUMP_FORWARD` | jump forward by oparg | 已实现 |
| `0x15` | 21 | `ECHO` | echo top stack value | 已实现 |
| `0x16` | 22 | `LOAD_ATTR` | load attribute of top variable | 已实现训练版 |
| `0x17` | 23 | `LOAD_BUILTIN_FUNC` / `LOAD_BUILTIN_FUN` | load builtin function by name | 已实现 |
| `0x19` | 25 | `LOAD_CONST` | load const and push stack | 已实现 |
| `0x1a` | 26 | `LOAD_CONST_VAR` | load const variable | 已实现 |
| `0x1b` | 27 | `LOAD_ENV_VAR` | load environment variable | 已实现 |
| `0x1c` | 28 | `LOAD_FUNCTION` | load function ref into stack | 已实现 |
| `0x1d` | 29 | `LOAD_FUNC_VARG` / `LOAD_FUN_VARG` | load function variable argument | 已实现 |
| `0x1e` | 30 | `LOAD_GLOBAL` / `LOAD_SPAN` | load global variable | 已实现为 global/builtin |
| `0x20` | 32 | `LOAD_LAMBDA` | load lambda function | 已实现 |
| `0x22` | 34 | `LOAD_NAME` | load named variable | 已实现 |
| `0x25` | 37 | `LOAD_SUBSCR` | load item by subscript | 已实现 |
| `0x26` | 38 | `LOAD_FLAW` | load flaw for catch | 已实现训练版 |
| `0x27` | 39 | `MAKE_FUNCTION` | make stack top become function | 已实现 |
| `0x28` | 40 | `PARSE_MODULE_ATTRS` | parse module attrs | 已实现训练版 |
| `0x29` | 41 | `POP_JUMP_IF_FALSE` | pop and jump if false | 已实现 |
| `0x2a` | 42 | `POP_JUMP_IF_TRUE` | pop and jump if true | 已实现 |
| `0x2b` | 43 | `POP_TOP` | pop stack top | 已实现 |
| `0x2c` | 44 | `RETURN_VALUE` | pop value and return | 已实现 |
| `0x2d` | 45 | `STORE_ATTR` | store attribute | 已实现训练版 |
| `0x2e` | 46 | `STORE_CONST_VAR` | store const variable | 已实现 |
| `0x2f` | 47 | `STORE_ENV_VAR` | store env variable | 已实现 |
| `0x30` | 48 | `STORE_GLOBAL` / `STORE_SPAN` | store global variable | 已实现 |
| `0x32` | 50 | `STORE_NAME` | store named variable | 已实现 |
| `0x34` | 52 | `STORE_SCRIPT` / `STORE_CODEBLOCK` | store code block | 已实现训练版 |
| `0x36` | 54 | `STORE_SUBSCR` | store item by subscript | 已实现 |
| `0x37` | 55 | `TO_BOOL` / `CONVERT_BOOL` | convert top to bool | 已实现 |
| `0x39` | 57 | `THROW` | throw value | 已实现训练版 |
| `0x3a` | 58 | `CATCH` | catch value | 已实现训练版 |
| `0x3b` | 59 | `REPLACE_FUNCTION` | replace function | 已实现 |
| `0x3c` | 60 | `JUMP_IF_FALSE_OR_POP` | jump if false, pop if true | 已实现 |
| `0x3d` | 61 | `JUMP_IF_TRUE_OR_POP` | jump if true, pop if false | 已实现 |
| `0x3e` | 62 | `UNARY_OP` | unary operator | 已实现，除 `UOP_UNKNOWN` |
| `0x3f` | 63 | `SLICE` | slice list | 已实现 |
| `0x40` | 64 | `UNLET_ATTR` | unlet attr | 已实现 |
| `0x41` | 65 | `UNLET_GLOBAL` / `UNLET_SPAN` | unlet global | 已实现 |
| `0x42` | 66 | `UNLET_NAME` | unlet local variable | 已实现 |
| `0x44` | 68 | `UNLET_SUBSCR` | unlet subscript item | 已实现 |
| `0x45` | 69 | `UNPACK_LIST` | unpack list | 已实现 |
| `0x46` | 70 | `UNPACK_DICT` | unpack dict | 已实现 |
| `0x47` | 71 | `INSTANCE_OF` | instance_of operation | 已实现训练版 |
| `0x48` | 72 | `INSTANCE_NOT` | instance_not operation | 已实现训练版 |
| `0x49` | 73 | `DEFER_CALL` | defer call | 已实现训练版 |
| `0x4a` | 74 | `LOAD_NONE` | load none placeholder | 已实现 |
| `0x4b` | 75 | `LOAD_NAMED_PARAM` | load named parameter | 已实现训练版 |
| `0x4c` | 76 | `BIND_NAMED_PARAM` | bind top value to named param | 已实现训练版 |
| `0x4d` | 77 | `LOAD_NOTE` | load note name from stack | 已实现训练版 |
| `0x4e` | 78 | `TYPE_CAST` | forced type conversion | 已实现 |
| `0x51` | 81 | `SET_FQN` | set scoped script in frame | 已实现训练版 |
| `0x52` | 82 | `UNSET_FQN` | reset scoped script | 已实现训练版 |
| `0x53` | 83 | `TENNIS_CALL` | tennis call | 已实现为 `CALL` |
| `0x54` | 84 | `EXEC_NETABLOCK` | execute neta block | 已实现训练版 |
| `0x5a` | 90 | `EXTENDED_ARG` | extended argument | 已实现训练版，作为 `CALL` 参数扩展 |
| `0x5d` | 93 | `GET_PATH_BY_FQN` | get path by fqn | 已实现训练版 |
| `0x5e` | 94 | `EXECUTE` | exec command | 已实现为元数据对象，不执行系统命令 |
| `0x5f` | 95 | `SOURCE` | source command | 已实现训练版 |
| `0x61` | 97 | `UNLET_CONST_VAR` | unlet constvar | 已实现 |
| `0x62` | 98 | `UNLET_VARIABLE` | unlet variable with scope | 已实现 |
| `0x63` | 99 | `LOAD_VARIABLE` | load variable with scope | 已实现 |
| `0x64` | 100 | `STORE_VARIABLE` | store variable with scope | 已实现 |
| `0x66` | 102 | `STORE_TYPE` | store type variable | 已实现训练版 |
| `0x67` | 103 | `YIELD_VALUE` | yield function value | 已实现训练版 |
| `0x68` | 104 | `RETURN_GENERATOR` | return generator | 已实现训练版 |
| `0x69` | 105 | `BUILD_GENERICS` | build generics | 已实现训练版 |
| `0x6a` | 106 | `BUILD_RECORD` | build record | 已实现训练版 |
| `0x6b` | 107 | `GENERICS_INSTANCE` | instance generics | 已实现训练版 |
| `0x6e` | 110 | `LOAD_IMPLICIT_VARIABLE` | get implicit variable | 已实现训练版 |

未知指令可以 dump/disasm；执行到非法 opcode 时会抛带 frame/ip/oparg/stack/locals 的 RuntimeError。
