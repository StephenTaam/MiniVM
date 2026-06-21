# STT Metadata VM 指令集参考

这份文档描述当前训练 VM 支持的 metadata 指令格式、`oparg` 解释规则、栈变化和执行语义。

如果想从“怎么写一个 JSON 程序”开始看，先读 [LANGUAGE_VM_GUIDE.md](LANGUAGE_VM_GUIDE.md)。如果想对照原始资料里的 opcode 数值，读 [SOURCE_COMPAT.md](SOURCE_COMPAT.md)。

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

也支持资料里的数字 opcode：

```json
{ "opcode": 25, "arg": 0 }
```

其中 `25` 是 `0x19`，对应资料表里的 `LOAD_CONST`。JSON 标准不支持 `0x19` 字面量，所以文件里写十进制。

推荐训练和调试时使用字符串 `op`，可读性更好。

资料对照的完整 opcode 数值表见 [SOURCE_COMPAT.md](SOURCE_COMPAT.md)。

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
| `Unknown` | 外部未知或未来扩展 opcode，当前 registry 内正常指令不应出现 | 执行未知 opcode 会报错 |

`arg` 也可以写成资料风格字段名 `oparg`：

```json
{ "op": "LOAD_CONST", "oparg": 0 }
```

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

资料数值写法：

```json
{ "opcode": 25, "oparg": 0 }
```

因为 `0x19 / 25` 对应 `LOAD_CONST`。

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
2. 当前 frame.constVars / envVars / typeVars / scripts
3. VM.globals / constGlobals / envGlobals / typeGlobals / scripts / modules
4. VM.builtins
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

### LOAD_VARIABLE / STORE_VARIABLE

```text
LOAD_VARIABLE arg
STORE_VARIABLE arg
oparg kind: NameIndex
```

资料表里描述为 “load/store variable with scope”。训练 VM 约定 `names[arg]` 可以写普通名字，也可以写显式作用域名字：

| 写法 | 表 |
| --- | --- |
| `x` | 默认当前 `frame.locals`，读取时也会继续查全局表 |
| `local:x` / `name:x` / `l:x` | 当前 `frame.locals` |
| `global:x` / `span:x` / `g:x` | `vm.globals` |
| `const:x` / `c:x` | `frame.constVars` 和 `vm.constGlobals` |
| `env:x` / `e:x` | `frame.envVars` 和 `vm.envGlobals` |
| `type:x` | `frame.typeVars` 和 `vm.typeGlobals` |
| `script:x` / `s:x` | `frame.scripts` 和 `vm.scripts` |
| `module:x` / `m:x` | VM 模块表，读取时会创建/返回同名模块 |

示例：

```json
{
  "constants": [99],
  "names": ["global:gx"],
  "instructions": [
    { "op": "LOAD_CONST", "arg": 0 },
    { "op": "STORE_VARIABLE", "arg": 0 },
    { "op": "LOAD_VARIABLE", "arg": 0 },
    { "op": "ECHO", "arg": 0 }
  ]
}
```

会输出 `99`。

### LOAD_TVAR / STORE_TVAR

```text
LOAD_TVAR arg
STORE_TVAR arg
oparg kind: NameIndex
```

训练版用 `frame.tvars` 模拟 原始运行时 的 `t:` 作用域：

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
2. 展开 EXTENDED_ARG，并收集 BIND_NAMED_PARAM 生成的命名参数
3. 弹出函数对象 / 内置函数 / class 对象
4. 函数调用时创建新 Frame
5. 把位置参数、命名参数和默认值绑定到 function_codeblock.argnames
6. 执行函数 CodeBlock
7. 把返回值压回调用者栈
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

`CALL` 也支持 class 对象。对 `DEFINE_CLASS` 产生的 class 执行 `CALL 0` 会创建一个实例并压栈，相当于 `CREATE_INSTANCE` 的简写。

### LOAD_NAMED_PARAM / BIND_NAMED_PARAM

```text
LOAD_NAMED_PARAM arg
BIND_NAMED_PARAM arg
oparg kind: NameIndex
```

命名参数由两步组成：

```text
LOAD_NAMED_PARAM c   -> push(named_param_marker("c"))
LOAD_CONST 3         -> push(3)
BIND_NAMED_PARAM c   -> pop value 和 marker，push named_param("c", 3)
```

之后 `CALL` 会把 `named_param` 从普通位置参数中分离出来，并按 `argnames` 绑定：

```text
[..., fn, 1, 2, named_param("c", 3)]
CALL 3
```

等价于：

```text
fn(1, 2, c=3)
```

### EXTENDED_ARG

```text
EXTENDED_ARG
oparg kind: None
```

训练 VM 把栈顶值包装成 `extended_arg`。`CALL` 收到它时：

```text
list  -> 展开成多个位置参数
dict  -> 展开成多个命名参数
其他值 -> 当作一个位置参数
```

示例：

```text
LOAD_NAME sum3
LOAD_CONST 1
LOAD_CONST 2
LOAD_CONST 3
BUILD_LIST 3
EXTENDED_ARG
CALL 1
```

等价于：

```text
sum3(1, 2, 3)
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

### LOAD_BUILTIN_FUNC / LOAD_BUILTIN_FUN

```text
LOAD_BUILTIN_FUNC arg
oparg kind: NameIndex
```

原始资料里也出现 `LOAD_BUILTIN_FUN`，当前 loader 会归一化成 `LOAD_BUILTIN_FUNC`。

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

## 资料指令别名

原始资料里有一些旧名或别名，当前 loader 会自动归一化：

| 资料/旧名 | 当前规范名 | 执行状态 |
| --- | --- | --- |
| `LOAD_BUILTIN_FUN` | `LOAD_BUILTIN_FUNC` | 已实现 |
| `LOAD_FUN_VARG` | `LOAD_FUNC_VARG` | 已实现，返回当前函数从 `arg` 开始的变长参数 list |
| `LOAD_SPAN` | `LOAD_GLOBAL` | 已实现为 global/builtin 查找 |
| `STORE_SPAN` | `STORE_GLOBAL` | 已实现为 globals 写入 |
| `STORE_CODEBLOCK` | `STORE_SCRIPT` | 已实现，把栈顶或 `blocks[arg]` 存入脚本表 |
| `CONVERT_BOOL` | `TO_BOOL` | 已实现 |
| `DEFINE_NOTE` | `DEFINE_ANNOTATION` | 已实现为注解元数据对象 |
| `UNLET_SPAN` | `UNLET_GLOBAL` | 已实现为删除 global |

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
| `6` | `NB_AND` | 整数按位与 |
| `7` | `NB_AND2` | 逻辑与 |
| `8` | `NB_OR` | 整数按位或 |
| `9` | `NB_OR2` | 逻辑或 |
| `10` | `NB_XOR` | 整数按位异或 |
| `11` | `NB_XOR2` | 逻辑异或 |
| `12` | `NB_LSHIFT` | 整数左移 |
| `13` | `NB_RSHIFT` | 整数右移 |
| `14` | `NB_AND3` | 训练版按整数按位与处理 |
| `15` | `NB_OR3` | 训练版按整数按位或处理 |
| `16` | `NB_XOR3` | 训练版按整数按位异或处理 |
| `17` | `NB_LSHIFT3` | 训练版按整数左移处理 |
| `18` | `NB_RSHIFT3` | 训练版按整数右移处理 |

`0 / NB_UNKNOWN` 保持非法子操作，执行会报错。

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
| `3` | `UOP_TILDE` | 整数按位取反 |
| `4` | `UOP_NOT` | 逻辑非 |
| `5` | `UOP_TILDE3` | 训练版按整数按位取反处理 |

`0 / UOP_UNKNOWN` 保持非法子操作，执行会报错。

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
| `7` | `CMP_PM` | pattern match：训练版用 `right` 作为正则/子串匹配 `left` |
| `8` | `CMP_RDM` | reverse pattern match：训练版用 `left` 作为正则/子串匹配 `right` |
| `9` | `CMP_IS` | 训练版按相等比较处理 |
| `10` | `CMP_ISNOT` | 训练版按不相等比较处理 |

`0 / CMP_UNKNOWN` 保持非法子操作，执行会报错。

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

`list` 和 `string` 支持负索引，例如 `-1` 表示最后一个元素。

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

`list[index] = value` 支持负索引。

### SLICE

```text
SLICE
oparg kind: None
```

语义：

```text
end = pop()
start = pop()
container = pop()
push(container[start:end])
```

支持：

```text
list[start:end]
string[start:end]
```

`start` / `end` 支持负数，并会被裁剪到容器边界。切片右边界不包含。

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

### ECHO

```text
ECHO
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

支持：

```text
instance.attrs[name]
class.attrs[name]
module.attrs[name]
dict[name]
```

实例属性查不到时，会尝试查 class 属性。`list`、`dict`、`string`、`iterator` 还支持只读属性 `len` / `length` / `size`；`function`、`builtin`、`class`、`module` 支持 `name`；`error` 支持 `name` 和 `payload`。

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

支持写入 `instance`、`class`、`module` 和 `dict`。

### UNLET_ATTR

```text
UNLET_ATTR arg
oparg kind: NameIndex
```

删除 `instance`、`class`、`module` 或 `dict` 上的属性/键。

## 模块、脚本和类型转换

### LOAD_MODULE / DEFINE_MODULE

```text
LOAD_MODULE arg
DEFINE_MODULE arg
oparg kind: NameIndex
```

`LOAD_MODULE` 按 `names[arg]` 或当前 `SET_FQN` 名称取模块。模块对象会缓存在 VM 模块表里，所以同名 `LOAD_MODULE` 会返回同一个模块，之前 `STORE_ATTR` 写进去的属性仍然存在。

`DEFINE_MODULE` 从栈顶取模块名；如果栈为空，则使用 `names[arg]` 或当前 frame 名称作为模块名，然后返回同名模块对象。

### LOAD_SCRIPT / STORE_SCRIPT / SOURCE

`STORE_SCRIPT` 把栈顶值保存到脚本表；如果栈为空且 `arg` 指向 `blocks[arg]`，会保存该子 CodeBlock 的函数值。

`LOAD_SCRIPT` 读取脚本表；如果没有保存过同名脚本，但存在同名 CodeBlock，则返回该 CodeBlock 的函数值；否则返回脚本名字符串。

`SOURCE` 不执行系统 shell。它会优先查找同名 CodeBlock 并执行；找不到时创建/返回同名模块对象，并记录到已加载模块表。

### TYPE_CAST

```text
TYPE_CAST arg
oparg kind: ConstIndex
```

支持两种写法：

```text
value, "int", TYPE_CAST      -> int(value)
value, TYPE_CAST arg         -> 优先用 constants[arg] 指定目标类型；如果 constants 越界再尝试 names[arg]
```

当前目标类型：

| 名称 | 结果 |
| --- | --- |
| `1` / `int` / `Int` | 整数 |
| `2` / `double` / `float` / `number` | 浮点数 |
| `3` / `string` / `String` | 字符串 |
| `4` / `bool` / `Bool` / `boolean` | 布尔值 |

## 原始资料中未明确的训练版语义

VM 已经为当前 registry 中的 opcode 都接了执行分支。少数指令依赖原始宿主环境或源码编译器上下文，训练版用可调试的本地语义代替：

| 类型 | 指令示例 | 当前语义 |
| --- | --- | --- |
| 宿主环境 | `EXECUTE`、`SOURCE`、`SOURCE_MODULE` | 不真正执行系统命令；返回元数据对象或加载本 VM 内 CodeBlock |
| 模块/FQN | `DECLARE_PACKAGE`、`DECLARE_IMPORT`、`SET_FQN`、`GET_PATH_BY_FQN` | 维护 frame/global 中的 package、module、路径映射信息 |
| 注解/泛型/记录 | `DEFINE_ANNOTATION`、`LOAD_NOTE`、`BUILD_GENERICS`、`BUILD_RECORD` | 返回 dict 形式的元数据对象 |
| 异常/Flaw | `DEFINE_FLAW`、`LOAD_FLAW`、`THROW`、`CATCH` | 用 VM 内部 `Error` 值模拟；有 `exceptiontable` 时跳 handler |
| 编译器书签 | `ENTER_LOOP`、`LEAVE_LOOP`、`EXIT_SCOPE`、`GARBAGE_COLLECT` | 作为 no-op/bookkeeping 指令处理 |
| 调用参数扩展 | `EXTENDED_ARG`、`LOAD_NAMED_PARAM`、`BIND_NAMED_PARAM` | 参与 `CALL` 的位置参数展开和命名参数绑定 |

如果执行到未知 opcode 或 `OPCODE_ILL`，会报：

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
  reason: illegal opcode / unknown opcode
```

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
    { "op": "ECHO", "arg": 0 }
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
    { "op": "ECHO", "arg": 0 },
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
build/stt_vm run your.json --dump-tables --trace
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
