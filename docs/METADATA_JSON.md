# Metadata JSON 格式说明

这份文档说明 `rhino_lab` 接收的 metadata JSON 文件结构、每个字段含义、默认值、引用关系和常见用法。

当前项目没有源码 parser，正式输入就是这里描述的静态 CodeBlock JSON。如何用这套 JSON 组织一段“程序”，见 [LANGUAGE_VM_GUIDE.md](LANGUAGE_VM_GUIDE.md)。

## 总体结构

一个 metadata 文件最外层是 Program：

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 0,
    "cbname": "main",
    "constants": [],
    "names": [],
    "blocks": [],
    "instructions": [],
    "exceptiontable": [],
    "instlnums": [],
    "baseline": 0
  }
}
```

最重要的关系是：

```text
instructions 里的 arg 会根据 opcode 去引用 constants / names / blocks 等表
```

例如：

```text
LOAD_CONST 0   -> 当前 CodeBlock.constants[0]
LOAD_NAME 2    -> 当前 CodeBlock.names[2]
DEFINE_FUN 0   -> 当前 CodeBlock.blocks[0]
BINARY_OP 1    -> binaryop_t.NB_ADD
CALL 2         -> 调用参数数量为 2
```

## 顶层 Program 字段

| 字段 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `version` | number | 否 | `1` | metadata 格式版本。当前 VM 只做记录，不按版本分支解析。 |
| `entry` | string | 否 | `""` | 入口 CodeBlock 名称。当前 `root` 有指令时默认执行 `root`；也可以用 CLI `--entry` 指定入口。 |
| `root` | object | 是 | 无 | 根 CodeBlock。所有执行、dump、递归 blocks 都从这里开始。 |

### entry 的使用

默认运行：

```bash
build/rhino_lab run program.json
```

指定入口：

```bash
build/rhino_lab run program.json --entry main
```

注意：当前实现中，如果 `root.instructions` 非空，默认会执行 `root`。如果想明确执行某个子 CodeBlock，建议使用 `--entry name`。

## CodeBlock 字段

CodeBlock 是 metadata 的核心结构。每个 CodeBlock 都有自己的静态表和指令数组。

资料中的 CodeBlock 类似 C 程序里的 Text/Data/Bss 分段，是所有 bytecode 块的基类；不同语法结构通过 `type` 扩展字段：

| 类名 | type | 用途 | 单个可执行文件可生成数量 |
| --- | --- | --- | --- |
| `CodeBlock` | `0` | Script 代码块，也就是最大层的脚本块 | 仅一个 |
| `FuncCodeBlock` | `1` | Function 代码块 | 多个 |
| `ClassCodeBlock` | `2` | Class 代码块 | 多个 |
| `NetaCodeBlock` | `4` | Neta 代码块 | 多个 |

```json
{
  "type": 1,
  "cbname": "main",
  "constants": [],
  "names": [],
  "blocks": [],
  "instructions": [],
  "exceptiontable": [],
  "instlnums": [],
  "baseline": 0,
  "currline": -1,
  "argnames": [],
  "defaults": [],
  "flags": [],
  "inherits": [],
  "annotations": [],
  "syntactic": {},
  "generics": []
}
```

| 字段 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `type` | number/string | 否 | `0` | CodeBlock 类型。资料里的规范写法是数字：`0` Script、`1` Function、`2` Class、`4` Neta；训练 JSON 也兼容字符串。 |
| `cbname` | string | 否 | `""` | CodeBlock 名称，用于 trace、dump、`--entry` 查找。 |
| `constants` | array | 否 | `[]` | 常量表。`LOAD_CONST arg` 会查这里。 |
| `names` / `name` | array<string> | 否 | `[]` | 名字表。资料示例有时写单数 `name`；loader 两种都支持。`LOAD_NAME`、`STORE_NAME`、`LOAD_GLOBAL` 等会查这里。 |
| `blocks` | array<object> | 否 | `[]` | 子 CodeBlock 表。`DEFINE_FUN`、`DEFINE_CLASS` 会查这里。 |
| `instructions` | array<object> | 否 | `[]` | 指令数组。VM 按 ip 顺序执行。 |
| `exceptiontable` | array<object> | 否 | `[]` | 异常处理表，资料备注为 Flaw。当前第一版只 dump 和校验，不参与执行。 |
| `instlnums` | array<number> | 否 | `[]` | 指令行号表。长度建议和 `instructions` 一致。 |
| `baseline` | number | 否 | `0` | 源码基准行号。当前只保存，不参与执行。 |
| `currline` | number | 否 | `-1` | 训练版保留字段；资料表里未列为核心静态字段。当前只保存。 |
| `argnames` | array<string> | 否 | `[]` | 函数参数名。函数 `CALL` 时按顺序绑定到 locals。 |
| `defaults` | array<Value> | 否 | `[]` | 默认参数值。当前按末尾参数默认值处理。 |
| `flags` | array<string> | 否 | `[]` | 函数标志。当前只保存，不参与执行。 |
| `inherits` | array<string> | 否 | `[]` | class 继承列表。当前只保存，不实现完整继承。 |
| `annotations` | array<object> | 否 | `[]` | 注解列表。当前只保存。 |
| `syntactic` | object/empty array | 否 | `{}` | ClassCodeBlock 扩展特性列表。资料表写作 Dict；示例里可能写空数组 `[]`，loader 支持 object 或空数组。 |
| `generics` | array<string> | 否 | `[]` | 泛型参数名。当前只保存。 |

### type

资料里的推荐写法是数字：

```text
0 -> CodeBlock / Script
1 -> FuncCodeBlock
2 -> ClassCodeBlock
4 -> NetaCodeBlock
```

训练 VM 也兼容字符串，但这只是为了方便手写和兼容旧示例：

```text
normal
script
function
func
class
neta
```

当前第一版中，`type` 主要用于 dump、trace 和创建 `FunctionValue` / `ClassType` 时展示。内部枚举值也直接使用资料里的 `0/1/2/4`。

### 按 CodeBlock 子类划分字段

资料里不同子类字段略有差异。训练 VM 使用统一结构读取，字段不存在时按空值处理。

| 字段 | CodeBlock type=0 | Func type=1 | Class type=2 | Neta type=4 |
| --- | --- | --- | --- | --- |
| `type` | 有 | 有 | 有 | 有 |
| `cbname` | 有 | 有 | 有 | 有 |
| `constants` | 有 | 有 | 有 | 有 |
| `names` | 有 | 有 | 有 | 有 |
| `blocks` | 有 | 有 | 有 | 有 |
| `instructions` | 有 | 有 | 有 | 有 |
| `exceptiontable` | 有 | 有 | 有 | 有 |
| `instlnums` | 有 | 有 | 有 | 有 |
| `baseline` | 有 | 有 | 有 | 有 |
| `argnames` | 无 | 有 | 通常无 | 通常无 |
| `defaults` | 无 | 有 | 通常无 | 通常无 |
| `flags` | 无 | 有，函数类型 | 通常无 | 通常无 |
| `inherits` | 无 | 无 | 有，继承列表 | 无 |
| `annotations` | 无 | 有，注解列表 | 有，注解列表 | 有，注解列表 |
| `syntactic` | 无 | 无 | 有，扩展特性列表 | 无 |
| `generics` | 无 | 有，泛型列表 | 有，泛型列表 | 通常无 |

### Bytecode 写入顺序和 JSON 表达

资料里的 bytecode 生成流程是：

```text
1. 先写入指令集版本号
2. 中序遍历 CodeBlock 树
3. 对每个 CodeBlock 依次写入 Name / Constants / Names / Instructions 等属性
4. 写 Constants 时如果遇到子 CodeBlock，则递归写入子 CodeBlock
```

训练 VM 当前使用 JSON，不直接写二进制 RSE。为了让 oparg 查表更直观，JSON 中建议把子块显式放进 `blocks`：

```text
DEFINE_CLASS 0  -> blocks[0]
DEFINE_FUN 1    -> blocks[1]
```

如果资料里的生成过程把某个子 CodeBlock 先写在 constants 遍历里，转换成训练 JSON 时仍建议归一化到 `blocks`，再通过 `DEFINE_CLASS` / `DEFINE_FUN` 等指令引用。

### cbname

建议每个 CodeBlock 都填写清楚：

```json
{
  "type": 1,
  "cbname": "add"
}
```

用途：

```text
1. trace 显示 frame 名称
2. dump 显示 CodeBlock 名称
3. --entry add 可以查找并执行该 CodeBlock
4. 函数值显示为 <function add>
```

### constants

常量表是数组，元素是 Value。

```json
"constants": [
  { "type": "int", "value": 1 },
  { "type": "string", "value": "hello" },
  { "type": "bool", "value": true }
]
```

指令引用：

```json
{ "op": "LOAD_CONST", "arg": 1 }
```

含义：

```text
push(constants[1])
```

### names

名字表是字符串数组。规范字段名是 `names`，资料中的部分生成示例写作 `name`，当前 loader 两种都支持：

```json
"names": ["a", "b", "result", "print"]
```

等价写法：

```json
"name": ["a", "b", "result", "print"]
```

常见引用：

```text
LOAD_NAME 0        -> names[0] = "a"
STORE_NAME 2       -> names[2] = "result"
LOAD_BUILTIN_FUNC 3 -> names[3] = "print"
```

名字表本身只保存名字，不保存变量值。变量值保存在运行时的：

```text
frame.locals
vm.globals
vm.builtins
```

### blocks

blocks 是子 CodeBlock 数组：

```json
"blocks": [
  {
    "type": 1,
    "cbname": "add",
    "argnames": ["a", "b"],
    "constants": [],
    "names": [],
    "blocks": [],
    "instructions": [
      { "op": "LOAD_ARG", "arg": 0 },
      { "op": "LOAD_ARG", "arg": 1 },
      { "op": "BINARY_OP", "arg": 1 },
      { "op": "RETURN_VALUE", "arg": 0 }
    ]
  }
]
```

指令引用：

```json
{ "op": "DEFINE_FUN", "arg": 0 }
```

含义：

```text
把 blocks[0] 创建成 FunctionValue 并压栈
```

## Value 字段

Value 有两种写法。

### 推荐写法：显式 typed value

```json
{ "type": "int", "value": 1 }
```

支持类型：

| type | value 类型 | 示例 |
| --- | --- | --- |
| `null` | 可省略 | `{ "type": "null" }` |
| `bool` | bool | `{ "type": "bool", "value": true }` |
| `int` | number | `{ "type": "int", "value": 123 }` |
| `double` | number | `{ "type": "double", "value": 3.14 }` |
| `float` | number | `{ "type": "float", "value": 3.14 }` |
| `number` | number | `{ "type": "number", "value": 3.14 }` |
| `string` | string | `{ "type": "string", "value": "hello" }` |
| `list` | array<Value> | `{ "type": "list", "value": [...] }` |
| `dict` | object<Value> | `{ "type": "dict", "value": {...} }` |

### 简写：直接 JSON 值

loader 也支持直接写普通 JSON 值：

```json
"constants": [
  1,
  2.5,
  true,
  null,
  "hello",
  [1, 2, 3]
]
```

不过训练时更推荐显式写 `type`，因为 dump 里更容易看懂类型来源。

### list 示例

```json
{
  "type": "list",
  "value": [
    { "type": "int", "value": 1 },
    { "type": "int", "value": 2 }
  ]
}
```

### dict 示例

```json
{
  "type": "dict",
  "value": {
    "name": { "type": "string", "value": "rhino" },
    "version": { "type": "int", "value": 1 }
  }
}
```

## Instruction 字段

Instruction 是 `instructions` 数组里的元素。

```json
{ "op": "LOAD_CONST", "arg": 0 }
```

完整字段：

| 字段 | 类型 | 必填 | 默认值 | 说明 |
| --- | --- | --- | --- | --- |
| `op` | string | `op`/`opcode` 二选一 | 无 | opcode 名称，推荐使用。 |
| `opcode` | number | `op`/`opcode` 二选一 | 无 | opcode 数字编号。可用于模拟真实 codegen 产物。 |
| `arg` | number | 否 | `0` | oparg。含义取决于 opcode。 |
| `oparg` | number | 否 | `0` | `arg` 的资料风格别名。`arg` 优先，缺省时读取 `oparg`。 |
| `target` | number | 否 | `-1` | 跳转目标 ip。跳转指令优先使用该字段。 |
| `line` | number | 否 | `-1` | 当前指令源码行号。当前只保存和展示。 |

### op 写法

推荐：

```json
{ "op": "LOAD_CONST", "arg": 0 }
```

好处：

```text
1. 可读性好
2. 不依赖 opcode 数字编号
3. 写 metadata 时不容易错
```

### opcode 写法

也支持：

```json
{ "opcode": 25, "arg": 0 }
```

loader 会根据内部 registry 把数字转回 opcode 名称。

当前 registry 已按资料截图中的 opcode 数值注册。例如：

| 十六进制 | 十进制 | 指令 |
| --- | --- | --- |
| `0x19` | `25` | `LOAD_CONST` |
| `0x22` | `34` | `LOAD_NAME` |
| `0x32` | `50` | `STORE_NAME` |
| `0x2c` | `44` | `RETURN_VALUE` |

JSON 标准不支持 `0x19` 这种十六进制字面量，所以文件里要写十进制：

```json
{ "opcode": 25, "oparg": 0 }
```

训练时如果不需要模拟真实 opcode id，仍建议使用字符串 `op`。

### arg

`arg` / `oparg` 是最重要字段，但它没有固定含义。

同样的 `arg: 0`，在不同指令下意思不同：

```text
LOAD_CONST 0   -> constants[0]
LOAD_NAME 0    -> names[0]
DEFINE_FUN 0   -> blocks[0]
LOAD_ARG 0     -> args[0]
BINARY_OP 0    -> NB_UNKNOWN
CALL 0         -> 0 个参数
```

### 资料风格指令名兼容

截图中的一些指令名和训练 VM 最初命名略有不同。loader 会把这些别名归一化到当前实现名：

| 资料/旧名 | 当前规范名 | 说明 |
| --- | --- | --- |
| `RHINO_ECHO` | `ECHO` | 旧名仍兼容；从栈顶弹出并输出 |
| `LOAD_BUILTIN_FUN` | `LOAD_BUILTIN_FUNC` | 加载内置函数 |
| `LOAD_FUN_VARG` | `LOAD_FUNC_VARG` | 加载函数变长参数，当前未实现执行 |
| `LOAD_SPAN` | `LOAD_GLOBAL` | 资料中替代 `LOAD_GLOBAL` 的命名 |
| `STORE_SPAN` | `STORE_GLOBAL` | 资料中替代 `STORE_GLOBAL` 的命名 |
| `STORE_CODEBLOCK` | `STORE_SCRIPT` | 资料中替代 `STORE_SCRIPT` 的命名，当前未实现执行 |
| `CONVERT_BOOL` | `TO_BOOL` | 转 bool |
| `DEFINE_NOTE` | `DEFINE_ANNOTATION` | 定义注解/Note |
| `UNLET_SPAN` | `UNLET_GLOBAL` | 资料中替代 `UNLET_GLOBAL` 的命名，当前未实现执行 |

### target

跳转指令可写：

```json
{ "op": "POP_JUMP_IF_FALSE", "arg": 10 }
```

也可写：

```json
{ "op": "POP_JUMP_IF_FALSE", "arg": 0, "target": 10 }
```

如果存在 `target`，VM 优先使用 `target`。

推荐训练时使用 `target`，因为它不容易受到插入指令影响。

## exceptiontable 字段

当前第一版只保留结构，不实现完整异常跳转。

格式：

```json
"exceptiontable": [
  {
    "startIp": 0,
    "endIp": 5,
    "handlerIp": 8,
    "type": "RuntimeError"
  }
]
```

字段：

| 字段 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| `startIp` | number | 否 | try 范围开始 ip。 |
| `endIp` | number | 否 | try 范围结束 ip。 |
| `handlerIp` | number | 否 | handler 指令 ip。 |
| `type` | string | 否 | 异常类型名。 |

兼容别名：

```text
start      -> startIp
end        -> endIp
handler    -> handlerIp
```

## annotations 字段

当前只保存，不参与执行。

格式：

```json
"annotations": [
  {
    "name": "deprecated",
    "value": { "type": "bool", "value": true }
  }
]
```

字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `name` | string | 注解名。 |
| `value` | Value | 注解值。 |

## syntactic 字段

当前只保存，不参与执行。

格式：

```json
"syntactic": {
  "source": { "type": "string", "value": "demo.phx" },
  "generated": { "type": "bool", "value": true }
}
```

值必须是 Value。

## 最小可运行示例

文件：

```text
examples/minimal.json
```

内容含义：

```text
a = 3
b = 2
c = a * b
echo c
```

核心结构：

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 1,
    "cbname": "main",
    "constants": [
      { "type": "int", "value": 3 },
      { "type": "int", "value": 2 }
    ],
    "names": ["a", "b", "c"],
    "blocks": [],
    "instructions": [
      { "op": "LOAD_CONST", "arg": 0 },
      { "op": "STORE_NAME", "arg": 0 },
      { "op": "LOAD_CONST", "arg": 1 },
      { "op": "STORE_NAME", "arg": 1 },
      { "op": "LOAD_NAME", "arg": 0 },
      { "op": "LOAD_NAME", "arg": 1 },
      { "op": "BINARY_OP", "arg": 3 },
      { "op": "STORE_NAME", "arg": 2 },
      { "op": "LOAD_NAME", "arg": 2 },
      { "op": "ECHO", "arg": 0 },
      { "op": "RETURN_VALUE", "arg": 0 }
    ],
    "exceptiontable": [],
    "instlnums": [],
    "baseline": 0,
    "currline": -1
  }
}
```

运行：

```bash
build/rhino_lab run examples/minimal.json --dump-tables --trace
```

输出中会看到：

```text
6
```

## 函数示例

文件：

```text
examples/function_call.json
```

含义：

```text
fun add(a, b) {
  return a + b
}

echo add(1, 2)
```

关键点：

```text
root.blocks[0] = add 函数 CodeBlock
DEFINE_FUN 0   -> 创建 add 函数对象
STORE_NAME 0   -> 保存到 names[0]，也就是 add
CALL 2         -> 调用 2 个参数
```

运行：

```bash
build/rhino_lab run examples/function_call.json --dump-tables --trace
```

## if 示例

下面这个片段表达：

```text
if 3 > 2 {
  echo "yes"
}
```

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 1,
    "cbname": "main",
    "constants": [
      { "type": "int", "value": 3 },
      { "type": "int", "value": 2 },
      { "type": "string", "value": "yes" }
    ],
    "names": [],
    "blocks": [],
    "instructions": [
      { "op": "LOAD_CONST", "arg": 0 },
      { "op": "LOAD_CONST", "arg": 1 },
      { "op": "COMPARE_OP", "arg": 5 },
      { "op": "POP_JUMP_IF_FALSE", "arg": 6 },
      { "op": "LOAD_CONST", "arg": 2 },
      { "op": "ECHO", "arg": 0 },
      { "op": "RETURN_VALUE", "arg": 0 }
    ]
  }
}
```

这里：

```text
COMPARE_OP 5             -> CMP_GT
POP_JUMP_IF_FALSE 6      -> 如果 false，跳到 ip=6
```

## while 示例

下面这个片段表达：

```text
i = 0
while i < 3 {
  echo i
  i = i + 1
}
```

```json
{
  "version": 1,
  "entry": "main",
  "root": {
    "type": 1,
    "cbname": "main",
    "constants": [
      { "type": "int", "value": 0 },
      { "type": "int", "value": 3 },
      { "type": "int", "value": 1 }
    ],
    "names": ["i"],
    "blocks": [],
    "instructions": [
      { "op": "LOAD_CONST", "arg": 0 },
      { "op": "STORE_NAME", "arg": 0 },

      { "op": "LOAD_NAME", "arg": 0 },
      { "op": "LOAD_CONST", "arg": 1 },
      { "op": "COMPARE_OP", "arg": 1 },
      { "op": "POP_JUMP_IF_FALSE", "arg": 13 },

      { "op": "LOAD_NAME", "arg": 0 },
      { "op": "ECHO", "arg": 0 },

      { "op": "LOAD_NAME", "arg": 0 },
      { "op": "LOAD_CONST", "arg": 2 },
      { "op": "BINARY_OP", "arg": 1 },
      { "op": "STORE_NAME", "arg": 0 },

      { "op": "JUMP_BACKWARD", "arg": 10 },
      { "op": "RETURN_VALUE", "arg": 0 }
    ]
  }
}
```

注意：

```text
JUMP_BACKWARD 10
```

在 ip=12 执行时，会跳回：

```text
12 - 10 = 2
```

也就是 while 条件判断位置。

## 编写 metadata 的推荐步骤

1. 先写伪代码。

```text
a = 1
b = 2
echo a + b
```

2. 把字面量放进 `constants`。

```json
"constants": [
  { "type": "int", "value": 1 },
  { "type": "int", "value": 2 }
]
```

3. 把变量名放进 `names`。

```json
"names": ["a", "b"]
```

4. 手写栈式指令。

```json
"instructions": [
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "STORE_NAME", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 1 },
  { "op": "STORE_NAME", "arg": 1 },
  { "op": "LOAD_NAME", "arg": 0 },
  { "op": "LOAD_NAME", "arg": 1 },
  { "op": "BINARY_OP", "arg": 1 },
  { "op": "ECHO", "arg": 0 },
  { "op": "RETURN_VALUE", "arg": 0 }
]
```

5. 用 trace 验证。

```bash
build/rhino_lab run your.json --dump-tables --trace
```

重点检查：

```text
lookup 是否查到了正确表项
before stack 是否是指令期望的栈形状
after stack 是否符合预期
locals/globals 是否正确变化
```

## 常见错误

### constants 下标越界

```json
"constants": [],
"instructions": [
  { "op": "LOAD_CONST", "arg": 0 }
]
```

会在 verifier 或 runtime 报：

```text
references missing constants[0]
```

### names 下标越界

```json
"names": [],
"instructions": [
  { "op": "STORE_NAME", "arg": 0 }
]
```

会报：

```text
references missing names[0]
```

### 栈数量不够

```json
"instructions": [
  { "op": "BINARY_OP", "arg": 1 }
]
```

`BINARY_OP` 需要两个操作数，会报：

```text
stack underflow
```

### CALL 栈形状不对

正确：

```text
[..., func, arg1, arg2]
CALL 2
```

错误：

```text
[..., arg1, arg2]
CALL 2
```

会因为找不到 function 对象而报错。

### 跳转目标越界

```json
{ "op": "POP_JUMP_IF_FALSE", "arg": 999 }
```

如果 `instructions` 没有 ip=999，会被 verifier 报出来。

## 与指令集文档的关系

本文解释 JSON 文件怎么写。

每条 opcode 的详细语义见：

```text
docs/INSTRUCTION_SET.md
```

日常写 metadata 时建议两个文档一起看：

```text
1. LANGUAGE_VM_GUIDE.md   看整体怎么写程序
2. METADATA_JSON.md       看字段怎么写
3. INSTRUCTION_SET.md     看指令怎么用
4. SOURCE_COMPAT.md       看资料截图如何对照
```
