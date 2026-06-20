# Rhino Lab 指令集参考

这份文档描述当前训练 VM 支持的 metadata 指令格式、`oparg` 解释规则、栈变化和执行语义。

## 基本模型

VM 不直接读取源码，只读取 codegen 后的 metadata：

```text
CodeBlock
  constants       常量表
  names           名字表
  blocks          子 CodeBlock 表
  instructions    指令数组
```

一条指令通常长这样：

```json
{ "op": "LOAD_CONST", "arg": 0 }
```

也支持数字 opcode：

```json
{ "opcode": 25, "arg": 0 }
```

推荐训练和调试时使用字符串 `op`，可读性更好。

## 栈记法

本文用下面的方式描述栈变化：

```text
before: [..., a, b]
after:  [..., result]
```

其中右侧是栈顶。例如：

```text
LOAD_CONST 0
before: [...]
after:  [..., constants[0]]
```

## oparg 类型

`arg` 的含义由 opcode 决定。

| oparg 类型 | 含义 | 示例 |
| --- | --- | --- |
| `None` | 不使用 `arg` | `RETURN_VALUE` |
| `ConstIndex` | 当前 CodeBlock 的 `constants[arg]` | `LOAD_CONST 0` |
| `NameIndex` | 当前 CodeBlock 的 `names[arg]` | `LOAD_NAME 2` |
| `BlockIndex` | 当前 CodeBlock 的 `blocks[arg]` | `DEFINE_FUN 0` |
| `JumpOffset` | 跳转偏移，或使用 `target` 字段 | `JUMP_FORWARD 3` |
| `JumpTarget` | 跳转目标 ip，或使用 `target` 字段 | `POP_JUMP_IF_FALSE 10` |
| `BinaryOp` | `binaryop_t` 枚举值 | `BINARY_OP 1` |
| `UnaryOp` | `unaryop_t` 枚举值 | `UNARY_OP 2` |
| `CompareOp` | `compareop_t` 枚举值 | `COMPARE_OP 3` |
| `ArgCount` | 调用参数数量 | `CALL 2` |
| `ImmInt` | 立即数整数 | `LOAD_IMM8 7` |
| `Unknown` | 已注册但未定义解释方式 | 未实现 opcode |

使用 `--trace` 时，VM 会打印：

```text
oparg kind: ConstIndex
lookup: constants[0]
lookup result: int(1)
```

## 常量和字面量

### LOAD_CONST

```text
LOAD_CONST arg
oparg kind: ConstIndex
```

语义：

```text
push(current.constants[arg])
```

栈变化：

```text
before: [...]
after:  [..., constants[arg]]
```

示例：

```json
{
  "constants": [{ "type": "int", "value": 1 }],
  "instructions": [
    { "op": "LOAD_CONST", "arg": 0 }
  ]
}
```

Trace 示例：

```text
LOAD_CONST 0 ; constants[0] = int(1)
```

### LOAD_IMM8

```text
LOAD_IMM8 arg
oparg kind: ImmInt
```

语义：

```text
push(int(arg))
```

示例：

```json
{ "op": "LOAD_IMM8", "arg": 42 }
```

等价于压入整数 `42`，不查 `constants` 表。

### LOAD_NONE

```text
LOAD_NONE
oparg kind: None
```

语义：

```text
push(null)
```

## 变量和作用域

### LOAD_NAME

```text
LOAD_NAME arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
value = resolveName(name)
push(value)
```

查找顺序：

```text
1. 当前 frame.locals
2. VM.globals
3. VM.builtins
```

找不到会报：

```text
NameError: name not found
```

示例：

```json
{
  "names": ["a"],
  "instructions": [
    { "op": "LOAD_NAME", "arg": 0 }
  ]
}
```

Trace：

```text
LOAD_NAME 0 ; names[0] = a
```

### STORE_NAME

```text
STORE_NAME arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
value = pop()
current.locals[name] = value
```

栈变化：

```text
before: [..., value]
after:  [...]
```

示例：

```json
[
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "STORE_NAME", "arg": 0 }
]
```

如果 `names[0] = "a"`，含义就是：

```text
a = constants[0]
```

### LOAD_GLOBAL

```text
LOAD_GLOBAL arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
push(globals[name] or builtins[name])
```

不查当前 `locals`。

### STORE_GLOBAL

```text
STORE_GLOBAL arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
globals[name] = pop()
```

### LOAD_VAR / STORE_VAR

```text
LOAD_VAR arg
STORE_VAR arg
oparg kind: NameIndex
```

训练版约定为当前局部作用域变量：

```text
LOAD_VAR  -> frame.locals[name]
STORE_VAR -> frame.locals[name] = pop()
```

### LOAD_GVAR / STORE_GVAR

```text
LOAD_GVAR arg
STORE_GVAR arg
oparg kind: NameIndex
```

训练版约定为 global 变量：

```text
LOAD_GVAR  -> vm.globals[name]
STORE_GVAR -> vm.globals[name] = pop()
```

### LOAD_TVAR / STORE_TVAR

```text
LOAD_TVAR arg
STORE_TVAR arg
oparg kind: NameIndex
```

训练版用 `frame.tvars` 模拟 Rhino/Phoenix 的 `t:` 作用域：

```text
LOAD_TVAR  -> frame.tvars[name]
STORE_TVAR -> frame.tvars[name] = pop()
```

### LOAD_BVAR / STORE_BVAR

```text
LOAD_BVAR arg
STORE_BVAR arg
oparg kind: NameIndex
```

训练版用 `frame.bvars` 模拟 `b:` 作用域：

```text
LOAD_BVAR  -> frame.bvars[name]
STORE_BVAR -> frame.bvars[name] = pop()
```

## 函数和调用

### DEFINE_FUN

```text
DEFINE_FUN arg
oparg kind: BlockIndex
```

语义：

```text
child = current.blocks[arg]
func = FunctionValue(child)
push(func)
```

常见组合：

```json
[
  { "op": "DEFINE_FUN", "arg": 0 },
  { "op": "STORE_NAME", "arg": 0 }
]
```

如果：

```text
blocks[0] = function add
names[0] = add
```

含义就是：

```text
add = <function add>
```

### DEFINE_FUNCTION 等价指令

以下指令当前都按 `DEFINE_FUN` 处理：

```text
DEFINE_FUNCTION
DEFINE_GFUN
DEFINE_TFUN
DEFINE_PRELOADER
DEFINE_CONSTRUCTOR
DEFINE_LAMBDA
DEFINE_CLOSURE
DEFINE_ANONYMOUS
```

### LOAD_ARG

```text
LOAD_ARG arg
oparg kind: ImmInt
```

语义：

```text
push(current.args[arg])
```

通常用于函数体内：

```json
[
  { "op": "LOAD_ARG", "arg": 0 },
  { "op": "LOAD_ARG", "arg": 1 },
  { "op": "BINARY_OP", "arg": 1 },
  { "op": "RETURN_VALUE", "arg": 0 }
]
```

### CALL

```text
CALL arg
oparg kind: ArgCount
```

`arg` 是参数数量。

调用前栈结构：

```text
before: [..., func, arg1, arg2, ..., argN]
```

执行过程：

```text
1. 弹出 N 个参数
2. 弹出函数对象
3. 创建新 Frame
4. 把参数绑定到 function_codeblock.argnames
5. 执行函数 CodeBlock
6. 把返回值压回调用者栈
```

栈变化：

```text
before: [..., func, arg1, arg2]
after:  [..., retval]
```

示例：

```json
[
  { "op": "LOAD_NAME", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 1 },
  { "op": "CALL", "arg": 2 }
]
```

如果 `names[0] = "add"`，含义就是：

```text
add(constants[0], constants[1])
```

### RETURN_VALUE

```text
RETURN_VALUE
oparg kind: None
```

语义：

```text
if stack 非空:
    retval = pop()
else:
    retval = null
退出当前 Frame
```

## 内置函数

### LOAD_BUILTIN_FUNC

```text
LOAD_BUILTIN_FUNC arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
push(builtins[name])
```

当前内置函数：

| 名称 | 参数 | 返回 | 说明 |
| --- | --- | --- | --- |
| `print` | 任意数量 | `null` | 打印参数 |
| `len` | 1 | `int` | 字符串/list/dict 长度 |
| `str` | 1 | `string` | 转字符串 |
| `int` | 1 | `int` | 转整数 |
| `bool` | 1 | `bool` | 转布尔 |

也可以通过 `LOAD_NAME print` 调用，因为 `LOAD_NAME` 会查 `builtins`。

## 算术和逻辑

### BINARY_OP

```text
BINARY_OP arg
oparg kind: BinaryOp
```

语义：

```text
right = pop()
left = pop()
push(binary(left, right, arg))
```

栈变化：

```text
before: [..., left, right]
after:  [..., result]
```

当前实现的 `binaryop_t`：

| arg | 名称 | 语义 |
| --- | --- | --- |
| `1` | `NB_ADD` | 加法；字符串参与时做字符串拼接 |
| `2` | `NB_SUBSTRACT` | 减法 |
| `3` | `NB_MULTIPLY` | 乘法 |
| `4` | `NB_DEVIDE` | 除法 |
| `5` | `NB_MOD` | 取模 |
| `7` | `NB_AND2` | 逻辑与 |
| `9` | `NB_OR2` | 逻辑或 |

已登记但未实现的常见值：

| arg | 名称 |
| --- | --- |
| `0` | `NB_UNKNOWN` |
| `6` | `NB_AND` |
| `8` | `NB_OR` |
| `10` | `NB_XOR` |
| `11` | `NB_XOR2` |
| `12` | `NB_LSHIFT` |
| `13` | `NB_RSHIFT` |
| `14` | `NB_AND3` |
| `15` | `NB_OR3` |
| `16` | `NB_XOR3` |
| `17` | `NB_LSHIFT3` |
| `18` | `NB_RSHIFT3` |

示例：

```json
[
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 1 },
  { "op": "BINARY_OP", "arg": 1 }
]
```

含义：

```text
constants[0] + constants[1]
```

### UNARY_OP

```text
UNARY_OP arg
oparg kind: UnaryOp
```

语义：

```text
value = pop()
push(unary(value, arg))
```

当前实现的 `unaryop_t`：

| arg | 名称 | 语义 |
| --- | --- | --- |
| `1` | `UOP_PLUS` | 正号 |
| `2` | `UOP_MINUS` | 负号 |
| `4` | `UOP_NOT` | 逻辑非 |

已登记但未实现：

| arg | 名称 |
| --- | --- |
| `0` | `UOP_UNKNOWN` |
| `3` | `UOP_TILDE` |
| `5` | `UOP_TILDE3` |

## 比较

### COMPARE_OP

```text
COMPARE_OP arg
oparg kind: CompareOp
```

语义：

```text
right = pop()
left = pop()
push(compare(left, right, arg))
```

当前实现的 `compareop_t`：

| arg | 名称 | 语义 |
| --- | --- | --- |
| `1` | `CMP_LT` | `<` |
| `2` | `CMP_LE` | `<=` |
| `3` | `CMP_EQ` | `==` |
| `4` | `CMP_NE` | `!=` |
| `5` | `CMP_GT` | `>` |
| `6` | `CMP_GE` | `>=` |
| `9` | `CMP_IS` | 训练版按相等比较处理 |
| `10` | `CMP_ISNOT` | 训练版按不相等比较处理 |

已登记但未实现：

| arg | 名称 |
| --- | --- |
| `0` | `CMP_UNKNOWN` |
| `7` | `CMP_PM` |
| `8` | `CMP_RDM` |

示例：

```json
[
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 1 },
  { "op": "COMPARE_OP", "arg": 5 }
]
```

含义：

```text
constants[0] > constants[1]
```

## 容器

### BUILD_LIST

```text
BUILD_LIST arg
oparg kind: ImmInt
```

`arg` 是元素数量。

语义：

```text
从栈顶弹出 arg 个值
按原顺序组成 list
push(list)
```

栈变化：

```text
before: [..., item1, item2, item3]
after:  [..., [item1, item2, item3]]
```

示例：

```json
[
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 1 },
  { "op": "BUILD_LIST", "arg": 2 }
]
```

### BUILD_DICT

```text
BUILD_DICT arg
oparg kind: ImmInt
```

`arg` 是 key/value 组数。

栈结构：

```text
before: [..., key1, value1, key2, value2]
after:  [..., {key1: value1, key2: value2}]
```

### LOAD_SUBSCR

```text
LOAD_SUBSCR
oparg kind: None
```

语义：

```text
index = pop()
container = pop()
push(container[index])
```

支持：

```text
list[index]
dict[key]
string[index]
```

### STORE_SUBSCR

```text
STORE_SUBSCR
oparg kind: None
```

语义：

```text
value = pop()
index = pop()
container = pop()
container[index] = value
```

支持：

```text
list[index] = value
dict[key] = value
```

## 跳转和条件

跳转指令支持可选 `target` 字段：

```json
{ "op": "JUMP_FORWARD", "arg": 3, "target": 10 }
```

如果存在 `target`，优先跳到 `target`。

### JUMP_FORWARD

```text
JUMP_FORWARD arg
oparg kind: JumpOffset
```

默认语义：

```text
ip = ip + arg
```

如果有 `target`：

```text
ip = target
```

### JUMP_BACKWARD

```text
JUMP_BACKWARD arg
oparg kind: JumpOffset
```

默认语义：

```text
ip = ip - arg
```

如果有 `target`：

```text
ip = target
```

### POP_JUMP_IF_FALSE

```text
POP_JUMP_IF_FALSE arg
oparg kind: JumpTarget
```

语义：

```text
cond = pop()
if !truthy(cond):
    ip = arg 或 target
```

### POP_JUMP_IF_TRUE

```text
POP_JUMP_IF_TRUE arg
oparg kind: JumpTarget
```

语义：

```text
cond = pop()
if truthy(cond):
    ip = arg 或 target
```

### JUMP_IF_FALSE_OR_POP

```text
JUMP_IF_FALSE_OR_POP arg
oparg kind: JumpTarget
```

语义：

```text
if !truthy(peek()):
    ip = arg 或 target
else:
    pop()
```

也就是说：条件为 false 时保留栈顶，条件为 true 时弹掉栈顶。

### JUMP_IF_TRUE_OR_POP

```text
JUMP_IF_TRUE_OR_POP arg
oparg kind: JumpTarget
```

语义：

```text
if truthy(peek()):
    ip = arg 或 target
else:
    pop()
```

也就是说：条件为 true 时保留栈顶，条件为 false 时弹掉栈顶。

### TO_BOOL

```text
TO_BOOL
oparg kind: None
```

语义：

```text
value = pop()
push(bool(value))
```

## 栈操作和输出

### POP_TOP

```text
POP_TOP
oparg kind: None
```

语义：

```text
pop()
```

### RHINO_ECHO

```text
RHINO_ECHO
oparg kind: None
```

语义：

```text
value = pop()
print(value)
```

栈变化：

```text
before: [..., value]
after:  [...]
```

## 类和对象

当前只实现训练版最小对象模型。

### DEFINE_CLASS

```text
DEFINE_CLASS arg
oparg kind: BlockIndex
```

语义：

```text
child = current.blocks[arg]
classValue = ClassType(child)
push(classValue)
```

### CREATE_INSTANCE

```text
CREATE_INSTANCE
oparg kind: None
```

语义：

```text
classValue = pop()
instance = new Instance(classValue)
push(instance)
```

### LOAD_ATTR

```text
LOAD_ATTR arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
obj = pop()
push(obj.attrs[name])
```

实例属性查不到时，会尝试查 class 属性。

### STORE_ATTR

```text
STORE_ATTR arg
oparg kind: NameIndex
```

语义：

```text
name = current.names[arg]
value = pop()
obj = pop()
obj.attrs[name] = value
```

## 已注册但未实现的 opcode

VM 注册了需求文档中的大部分 Rhino/Phoenix opcode 名称，方便 loader 和 disassembler 识别。

如果执行到未实现 opcode，会报：

```text
RuntimeError:
  frame: ...
  ip: ...
  instruction: ...
  oparg kind: ...
  lookup: ...
  lookup result: ...
  stack before: ...
  stack current: ...
  locals: ...
  reason: opcode not implemented: XXX
```

这类指令可以反汇编，但不能执行。

## 常见写法

### a = 1

```json
{
  "constants": [{ "type": "int", "value": 1 }],
  "names": ["a"],
  "instructions": [
    { "op": "LOAD_CONST", "arg": 0 },
    { "op": "STORE_NAME", "arg": 0 }
  ]
}
```

### echo a

```json
{
  "names": ["a"],
  "instructions": [
    { "op": "LOAD_NAME", "arg": 0 },
    { "op": "RHINO_ECHO", "arg": 0 }
  ]
}
```

### c = a + b

```json
{
  "names": ["a", "b", "c"],
  "instructions": [
    { "op": "LOAD_NAME", "arg": 0 },
    { "op": "LOAD_NAME", "arg": 1 },
    { "op": "BINARY_OP", "arg": 1 },
    { "op": "STORE_NAME", "arg": 2 }
  ]
}
```

### if a > b echo a

```json
{
  "names": ["a", "b"],
  "instructions": [
    { "op": "LOAD_NAME", "arg": 0 },
    { "op": "LOAD_NAME", "arg": 1 },
    { "op": "COMPARE_OP", "arg": 5 },
    { "op": "POP_JUMP_IF_FALSE", "arg": 7 },
    { "op": "LOAD_NAME", "arg": 0 },
    { "op": "RHINO_ECHO", "arg": 0 },
    { "op": "RETURN_VALUE", "arg": 0 },
    { "op": "RETURN_VALUE", "arg": 0 }
  ]
}
```

### add(1, 2)

参考项目内示例：

```text
examples/function_call.json
```

核心调用序列：

```json
[
  { "op": "LOAD_NAME", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 0 },
  { "op": "LOAD_CONST", "arg": 1 },
  { "op": "CALL", "arg": 2 }
]
```

## 调试建议

写 metadata 时建议一直带上：

```bash
build/rhino_lab run your.json --dump-tables --trace
```

重点看：

```text
oparg kind
lookup
lookup result
before stack
after stack
locals
globals
```

这几项能直接说明：

```text
当前 opcode 是什么
arg 被解释成什么
arg 查了哪张表
查表结果是什么
执行前后栈怎么变
变量环境怎么变
```
