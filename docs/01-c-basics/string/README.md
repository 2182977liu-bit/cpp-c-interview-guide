# 1.5 预处理与编译链接

> 作者：青松与桑叶 | 搞懂编译链接过程，才能真正理解C/C++程序的构建

---

## 目录

- [编译链接四阶段](#编译链接四阶段)
- [预处理](#预处理)
- [宏定义](#宏定义)
- [条件编译](#条件编译)
- [头文件包含](#头文件包含)
- [预定义宏](#预定义宏)
- [常见面试题](#常见面试题)
- [踩坑提醒](#踩坑提醒)
- [完整示例代码](#完整示例代码)

---

## 编译链接四阶段

从你写完 `.c` 文件到生成可执行文件，中间经历了四个阶段。理解这个过程是面试高频考点。

```
源代码 hello.c
    │
    ▼
┌─────────────┐
│  1. 预处理   │  gcc -E hello.c -o hello.i
│  (Preprocess)│  处理 # 开头的指令
└──────┬──────┘
       │ hello.i（纯C代码，没有宏和条件编译）
       ▼
┌─────────────┐
│  2. 编译     │  gcc -S hello.i -o hello.s
│  (Compile)   │  语法分析 → 语义分析 → 优化 → 生成汇编
└──────┬──────┘
       │ hello.s（汇编代码）
       ▼
┌─────────────┐
│  3. 汇编     │  gcc -c hello.s -o hello.o
│  (Assemble)  │  汇编代码 → 机器码
└──────┬──────┘
       │ hello.o（目标文件，二进制）
       ▼
┌─────────────┐
│  4. 链接     │  gcc hello.o -o hello
│  (Link)      │  合并目标文件 + 链接库 → 可执行文件
└──────┬──────┘
       │ hello（可执行文件）
       ▼
    运行 ./hello
```

### 一键完成

```bash
gcc -Wall hello.c -o hello
# 等价于依次执行上面四个步骤
```

### 各阶段详解

#### 阶段1：预处理（Preprocessing）

预处理器处理所有以 `#` 开头的指令：

- 展开所有 `#include`（把头文件内容插入）
- 替换所有宏定义（`#define`）
- 处理条件编译（`#if`、`#ifdef` 等）
- 删除所有注释

```bash
# 查看预处理结果
gcc -E hello.c -o hello.i
```

#### 阶段2：编译（Compilation）

编译器将预处理后的纯C代码转换为汇编代码：

- 词法分析：把代码拆成 token
- 语法分析：检查语法是否正确，构建语法树
- 语义分析：检查类型是否匹配
- 中间代码生成 + 优化
- 生成汇编代码

```bash
# 查看汇编代码
gcc -S hello.c -o hello.s
```

#### 阶段3：汇编（Assembly）

汇编器将汇编代码转换为机器码（目标文件）：

```bash
# 生成目标文件
gcc -c hello.c -o hello.o
```

目标文件（`.o` 文件）里面是机器码，但还不能直接运行，因为里面引用的外部函数（如 `printf`）还没有找到实现。

#### 阶段4：链接（Linking）

链接器将多个目标文件和库文件合并成可执行文件：

- **符号解析**：找到每个符号（函数、变量）的定义
- **地址重定位**：为每个符号分配最终地址
- **合并段**：将各目标文件的相同段合并

```bash
# 链接生成可执行文件
gcc hello.o -o hello
```

### 静态链接 vs 动态链接

| 特性 | 静态链接 | 动态链接 |
|------|---------|---------|
| 库文件 | `.a`（Linux）/ `.lib`（Windows） | `.so`（Linux）/ `.dll`（Windows） |
| 方式 | 编译时将库代码复制到可执行文件 | 运行时加载库文件 |
| 优点 | 不依赖外部库，部署简单 | 文件小，多程序可共享库 |
| 缺点 | 文件大，更新库需要重新编译 | 需要依赖库存在 |
| 编译选项 | `-static` | 默认 |

```bash
# 静态链接
gcc hello.c -static -o hello

# 动态链接（默认）
gcc hello.c -o hello

# 查看程序依赖的动态库
ldd ./hello
```

## 预处理

预处理是编译的第一步，由预处理器（cpp）完成。所有预处理指令都以 `#` 开头。

### #include — 头文件包含

```c
#include <stdio.h>   // 在系统目录中查找
#include "myheader.h" // 先在当前目录查找，找不到再去系统目录
```

`<>` 用于系统头文件，`""` 用于自定义头文件。

### #define — 宏定义

详见下一节。

### #if / #ifdef / #ifndef — 条件编译

详见条件编译章节。

### #pragma — 编译器指令

```c
#pragma once           // 头文件只包含一次（现代编译器支持）
#pragma pack(1)        // 设置1字节对齐
#pragma warning(disable: 4996)  // MSVC 禁用特定警告
```

## 宏定义

宏定义是最常用的预处理指令，本质是**文本替换**。

### 对象宏（无参数宏）

```c
#define PI 3.14159
#define MAX_SIZE 100
#define DEBUG

double area = PI * r * r;  // 预处理后: double area = 3.14159 * r * r;
int arr[MAX_SIZE];          // 预处理后: int arr[100];
```

### 函数宏（带参数宏）

```c
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))

int m = MAX(3, 5);       // 预处理后: int m = ((3) > (5) ? (3) : (5));
int s = SQUARE(3 + 1);   // 预处理后: int s = ((3 + 1) * (3 + 1));
                         // 如果不加括号: 3 + 1 * 3 + 1 = 7（错误！）
```

**为什么参数要加括号？** 因为宏是纯文本替换，不是函数调用。看这个例子：

```c
// 错误写法
#define SQUARE(x) x * x
int s = SQUARE(3 + 1);   // 替换后: 3 + 1 * 3 + 1 = 7（不是16！）

// 正确写法
#define SQUARE(x) ((x) * (x))
int s = SQUARE(3 + 1);   // 替换后: ((3 + 1) * (3 + 1)) = 16
```

**为什么整体也要加括号？**

```c
#define SQUARE(x) (x) * (x)
int s = 2 * SQUARE(3);   // 替换后: 2 * (3) * (3) = 18（不是36！）

// 正确写法
#define SQUARE(x) ((x) * (x))
int s = 2 * SQUARE(3);   // 替换后: 2 * ((3) * (3)) = 18... 还是不对
// 这种情况需要调用者自己加括号: 2 * (SQUARE(3))
```

### 宏的常见用途

```c
// 1. 常量定义
#define PI 3.14159
#define BUFFER_SIZE 1024

// 2. 日志输出
#define LOG(msg) printf("[INFO] %s:%d: %s\n", __FILE__, __LINE__, msg)

// 3. 调试开关
#ifdef DEBUG
    #define DEBUG_PRINT(fmt, ...) printf(fmt, ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...)  // 调试模式下为空
#endif

// 4. 代码块宏（用 do { } while(0) 包裹）
#define SWAP(a, b) do { \
    int temp = a;        \
    a = b;               \
    b = temp;            \
} while(0)
```

### 宏 vs 函数 vs inline

| 特性 | 宏 | 函数 | inline函数 |
|------|-----|------|-----------|
| 处理时机 | 预处理（文本替换） | 编译 | 编译 |
| 类型检查 | 无 | 有 | 有 |
| 调试 | 困难（展开后看不到） | 容易 | 容易 |
| 代码膨胀 | 可能严重 | 无 | 可能轻微 |
| 副作用 | 容易有（参数多次求值） | 无 | 无 |
| 适用场景 | 简单的常量和条件编译 | 一般逻辑 | 短小且频繁调用的函数 |

**现代C++推荐：** 能用 `const`/`constexpr` 就不用宏定义常量，能用 `inline` 函数就不用函数宏。

## 条件编译

条件编译让你可以根据条件选择性地编译代码，常用于跨平台、调试、功能开关等场景。

### #ifdef / #ifndef / #endif

```c
#define DEBUG

#ifdef DEBUG
    printf("调试模式\n");    // 如果定义了 DEBUG，编译这段代码
#endif

#ifndef RELEASE
    printf("非发布模式\n");  // 如果没有定义 RELEASE，编译这段代码
#endif
```

### #if / #elif / #else

```c
#define VERSION 2

#if VERSION == 1
    printf("版本1\n");
#elif VERSION == 2
    printf("版本2\n");
#else
    printf("未知版本\n");
#endif
```

### defined 运算符

```c
#if defined(DEBUG) && defined(VERBOSE)
    printf("详细调试信息\n");
#endif
```

### 防止头文件重复包含

```c
// 方法1：传统宏卫士（兼容性最好）
#ifndef MYHEADER_H
#define MYHEADER_H

// 头文件内容

#endif

// 方法2：#pragma once（更简洁，现代编译器支持）
#pragma once

// 头文件内容
```

### 跨平台代码

```c
#ifdef _WIN32
    #include <windows.h>
#elif defined(__linux__)
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
#endif
```

## 头文件包含

### 头文件里应该放什么

- 函数声明（不是定义）
- 类型定义（struct、typedef、enum）
- 宏定义和常量
- 内联函数（C++）
- 模板定义（C++）

### 头文件里不应该放什么

- 函数定义（普通函数）
- 变量定义（用 extern 声明，在 .c 文件中定义）
- using 命名空间（C++）

### extern 关键字

```c
// header.h
extern int global_var;  // 声明：告诉编译器这个变量存在，但不分配内存

// source.c
int global_var = 42;    // 定义：真正分配内存
```

## 预定义宏

编译器预定义了一些有用的宏，可以直接使用：

```c
#include <stdio.h>

int main() {
    printf("当前文件: %s\n", __FILE__);       // 源文件名
    printf("当前行号: %d\n", __LINE__);       // 当前行号
    printf("编译日期: %s\n", __DATE__);       // 编译日期
    printf("编译时间: %s\n", __TIME__);       // 编译时间
    printf("是否C编译器: %d\n", __STDC__);    // 是否标准C

    // C99 新增
    printf("函数名: %s\n", __func__);         // 当前函数名

    return 0;
}
```

## 常见面试题

### 面试题1：C/C++ 编译链接的过程？

**标准答案：**

> 编译链接分为四个阶段：
> 1. **预处理**：处理 `#` 开头的指令，包括宏展开、头文件包含、条件编译、删除注释
> 2. **编译**：词法分析、语法分析、语义分析、代码优化，生成汇编代码
> 3. **汇编**：将汇编代码转换为机器码，生成目标文件（.o）
> 4. **链接**：合并多个目标文件，解析外部符号引用，生成可执行文件
>
> 链接又分为静态链接和动态链接。静态链接将库代码复制到可执行文件中，动态链接在运行时加载共享库。

### 面试题2：头文件为什么要加 #ifndef 保护？

**标准答案：**

> 因为一个头文件可能被多个源文件包含，也可能在同一个文件中被间接包含多次。如果不加保护，头文件中的内容（如类型定义、函数声明）会被重复处理，导致重复定义的编译错误。
>
> `#ifndef` 宏卫士通过检查一个唯一的宏是否已定义来避免重复包含。现代编译器也支持 `#pragma once`，效果相同但更简洁。

### 面试题3：宏定义和函数的区别？

**标准答案：**

> 核心区别：
> 1. 宏是预处理阶段的文本替换，函数是编译阶段的调用
> 2. 宏没有类型检查，函数有完整的类型检查
> 3. 宏可能导致代码膨胀（每次使用都展开一份），函数不会
> 4. 宏的参数可能有副作用（如 `SQUARE(x++)` 会执行多次 `x++`），函数不会
> 5. 宏不能调试（展开后看不到宏名），函数可以单步调试
>
> 现代C++推荐用 `inline` 函数和 `constexpr` 替代宏。

### 面试题4：define 和 typedef 的区别？

**标准答案：**

> `#define` 是预处理指令，纯文本替换，没有类型检查。`typedef` 是C语言关键字，在编译阶段处理，有类型检查。
>
> ```c
> #define INT_PTR int*
> typedef int* IntPtr;
>
> INT_PTR a, b;   // a 是 int*，b 是 int（宏展开后: int* a, b）
> IntPtr c, d;    // c 和 d 都是 int*
> ```
>
> 推荐用 `typedef`，更安全。

### 面试题5：静态链接和动态链接的区别？

**标准答案：**

> 静态链接在编译时将库的代码复制到可执行文件中，优点是不依赖外部库，缺点是文件大、更新库需要重新编译。动态链接在运行时加载共享库（.so/.dll），优点是文件小、多程序共享库，缺点是运行时需要库文件存在。

## 踩坑提醒

### 坑1：宏的参数副作用

```c
#define SQUARE(x) ((x) * (x))
int a = 3;
int s = SQUARE(a++);  // a 被递增了两次！替换后: ((a++) * (a++))
```

**正确做法：** 宏参数不要用有副作用的表达式。或者用 inline 函数替代。

### 坑2：宏的多行定义忘记 `\`

```c
#define LOG(msg) \
    printf("[INFO] %s:%d: %s\n", __FILE__, __LINE__, msg)
// 每行末尾（最后一行除外）必须有反斜杠 \
```

### 坑3：头文件里定义变量

```c
// 错误！如果这个头文件被多个 .c 文件包含，会导致重复定义
// header.h
int global_var = 42;

// 正确做法
// header.h
extern int global_var;  // 声明

// source.c
int global_var = 42;    // 定义
```

### 坑4：#define 没有作用域

```c
void func1() {
    #define X 10
}
void func2() {
    printf("%d\n", X);  // X 在这里也能用！宏没有作用域概念
}
```

## 完整示例代码

所有示例代码可以在 [preprocessor_demo.c](../../../code/01-c-basics/preprocessor_demo.c) 中找到，编译运行：

```bash
gcc -Wall -o preprocessor_demo preprocessor_demo.c && ./preprocessor_demo
```

---

> 上一节：[1.4 struct与union](../struct/README.md) | 下一节：[2.1 类与对象](../../02-cpp-oop/class/README.md)
