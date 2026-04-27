# 1.3 内存管理

> 作者：青松与桑叶 | 内存管理是C/C++程序员的核心能力，搞懂它才能写出安全的代码

---

## 目录

- [程序中的内存区域](#程序中的内存区域)
- [栈与堆的区别](#栈与堆的区别)
- [malloc / calloc / realloc / free](#malloc--calloc--realloc--free)
- [new / delete（C++）](#new--deletec)
- [内存泄漏](#内存泄漏)
- [内存对齐](#内存对齐)
- [常见面试题](#常见面试题)
- [踩坑提醒](#踩坑提醒)
- [完整示例代码](#完整示例代码)

---

## 程序中的内存区域

一个C/C++程序的内存被分为几个区域，每个区域有不同的用途和生命周期。

```
程序内存布局（从低地址到高地址）：

┌─────────────────────────────┐  高地址
│         栈区 (Stack)         │  ← 局部变量、函数参数，由编译器自动管理
│         ↓ 向下增长 ↓         │
├─────────────────────────────┤
│                             │
│      空闲区域 (Free)         │
│                             │
├─────────────────────────────┤
│         ↑ 向上增长 ↑         │
│         堆区 (Heap)          │  ← malloc/new 分配，由程序员手动管理
├─────────────────────────────┤
│       BSS 段                 │  ← 未初始化的全局/静态变量
├─────────────────────────────┤
│       数据段 (Data)          │  ← 已初始化的全局/静态变量
├─────────────────────────────┤
│       代码段 (Text)          │  ← 程序的机器指令，只读
└─────────────────────────────┘  低地址
```

简单记：
- **代码段**：存放程序的指令，只读
- **数据段**：存放已初始化的全局变量和静态变量
- **BSS段**：存放未初始化的全局变量和静态变量（自动初始化为0）
- **堆**：手动分配，手动释放
- **栈**：自动分配，自动释放

## 栈与堆的区别

这是面试必考题，一定要背熟。

| 特性 | 栈 (Stack) | 堆 (Heap) |
|------|-----------|-----------|
| 分配方式 | 编译器自动分配释放 | 程序员手动分配释放 |
| 分配速度 | 非常快（指针移动即可） | 较慢（需要查找空闲块） |
| 空间大小 | 较小（通常1MB~8MB） | 较大（取决于物理内存和虚拟内存） |
| 生长方向 | 从高地址向低地址增长 | 从低地址向高地址增长 |
| 内存连续性 | 连续内存 | 不一定连续 |
| 碎片问题 | 无碎片 | 容易产生碎片 |
| 生命周期 | 函数调用结束自动释放 | 需要手动 free/delete |
| 安全性 | 较高（自动管理） | 较低（容易泄漏、越界） |
| 典型用途 | 局部变量、函数参数、返回地址 | 动态数据结构（链表、树等）、大对象 |

### 栈空间示例

```c
void func() {
    int a = 10;          // 栈上分配
    int b = 20;          // 栈上分配
    int arr[100];        // 栈上分配（注意：别在栈上放太大的数组！）
}   // 函数结束，a、b、arr 自动释放

int main() {
    func();
    // a、b、arr 已经不存在了
    return 0;
}
```

### 堆空间示例

```c
void func() {
    int *p = (int *)malloc(sizeof(int) * 100);  // 堆上分配
    // 使用 p ...
    free(p);  // 必须手动释放！
    p = NULL; // 好习惯：释放后置空
}
```

## malloc / calloc / realloc / free

这四个函数是C语言动态内存管理的核心，都在 `<stdlib.h>` 中。

### malloc — 分配内存

```c
void *malloc(size_t size);
```

- 分配 `size` 字节的内存
- 返回 `void*`，需要强制类型转换
- **不会初始化内存**，内容是随机的（脏数据）
- 分配失败返回 `NULL`

```c
int *p = (int *)malloc(sizeof(int) * 10);  // 分配10个int
if (p == NULL) {
    printf("内存分配失败！\n");
    return -1;
}
// 使用 p[0] ~ p[9]
free(p);
```

### calloc — 分配并清零

```c
void *calloc(size_t num, size_t size);
```

- 分配 `num * size` 字节的内存
- **自动初始化为 0**（和 malloc 的关键区别）
- 分配失败返回 `NULL`

```c
int *p = (int *)calloc(10, sizeof(int));  // 分配10个int，全部初始化为0
if (p == NULL) {
    printf("内存分配失败！\n");
    return -1;
}
// p[0] ~ p[9] 都是 0
free(p);
```

### realloc — 调整内存大小

```c
void *realloc(void *ptr, size_t size);
```

- 调整 `ptr` 指向的内存块大小为 `size`
- 如果新大小比原来大，可能移动到新位置（原来的内容会被复制过去）
- 如果 `ptr` 是 `NULL`，等价于 `malloc(size)`
- 如果 `size` 是 0，等价于 `free(ptr)`
- 分配失败返回 `NULL`，**原来的内存不会被释放**

```c
int *p = (int *)malloc(sizeof(int) * 5);  // 先分配5个
p[0] = 1; p[1] = 2; p[2] = 3;

// 需要更多空间，扩展到10个
int *new_p = (int *)realloc(p, sizeof(int) * 10);
if (new_p == NULL) {
    // 扩展失败，p 仍然有效，不要忘记释放
    free(p);
    return -1;
}
p = new_p;  // 扩展成功，使用新指针
free(p);
```

**踩坑提醒：** 不要直接 `p = realloc(p, new_size)`，如果 realloc 失败返回 NULL，原来的 p 就丢了，内存就泄漏了。

### free — 释放内存

```c
void free(void *ptr);
```

- 释放 `ptr` 指向的堆内存
- `free(NULL)` 是安全的，什么也不做
- **不能 free 栈上的内存**
- **不能 free 同一块内存两次**（double free）
- free 后最好把指针置为 NULL

```c
int *p = (int *)malloc(sizeof(int));
*p = 42;
free(p);
p = NULL;  // 好习惯，防止悬垂指针
```

## new / delete（C++）

C++ 提供了更高级的内存管理方式。

### 基本用法

```cpp
// 分配单个变量
int *p = new int(42);       // 分配一个int，初始化为42
delete p;
p = nullptr;

// 分配数组
int *arr = new int[10];     // 分配10个int
delete[] arr;               // 注意：数组要用 delete[]
arr = nullptr;
```

### malloc/free vs new/delete

| 特性 | malloc/free | new/delete |
|------|-------------|------------|
| 语言 | C | C++ |
| 类型安全 | 不安全（返回 void*） | 安全（自动计算大小） |
| 初始化 | 不会初始化 | 可以初始化（`new int(42)`） |
| 构造/析构 | 不会调用 | 会自动调用构造函数和析构函数 |
| 失败处理 | 返回 NULL | 抛出 std::bad_alloc 异常 |
| 重载 | 不能重载 | 可以重载 |
| 数组 | `malloc/free` | `new[]/delete[]` |

**重要规则：** new 和 delete 配对使用，new[] 和 delete[] 配对使用，**千万不要混用**。

```cpp
int *p = new int[10];
delete p;       // 错误！应该用 delete[]
delete[] p;     // 正确

int *q = (int *)malloc(sizeof(int) * 10);
delete q;       // 错误！malloc 要用 free
free(q);        // 正确
```

## 内存泄漏

内存泄漏就是分配了内存但忘记了释放，导致这块内存一直被占用，无法被再次使用。长期运行的程序如果存在内存泄漏，可用内存会越来越少，最终可能导致程序崩溃。

### 常见的内存泄漏场景

```c
// 场景1：忘记 free
void leak1() {
    int *p = (int *)malloc(sizeof(int));
    *p = 42;
    // 忘记 free(p) 了！
}

// 场景2：指针被覆盖
void leak2() {
    int *p = (int *)malloc(sizeof(int));
    p = (int *)malloc(sizeof(int));  // 第一块内存泄漏了！
    free(p);  // 只释放了第二块
}

// 场景3：异常/提前返回导致没有释放
void leak3() {
    int *p = (int *)malloc(sizeof(int));
    if (some_condition) {
        return;  // 提前返回，忘记 free(p)
    }
    free(p);
}

// 场景4：realloc 失败没处理
void leak4() {
    int *p = (int *)malloc(sizeof(int) * 10);
    p = (int *)realloc(p, sizeof(int) * 100);  // 如果失败，p 变成 NULL，原来的内存泄漏
    // 正确做法：用临时变量接收 realloc 的返回值
}
```

### 如何检测内存泄漏

1. **Valgrind**（Linux）：`valgrind --leak-check=full ./your_program`
2. **AddressSanitizer**（gcc/clang）：编译时加 `-fsanitize=address`
3. **Visual Studio**：内置内存诊断工具
4. **代码审查**：每个 malloc 都要配一个 free

### 预防内存泄漏的技巧

```c
// 技巧1：谁分配谁释放
void func() {
    int *p = (int *)malloc(sizeof(int));
    // ... 使用 p ...
    free(p);  // 在同一个函数里释放
}

// 技巧2：使用 goto 统一清理（C语言中 goto 的合法用法之一）
int func() {
    int *p1 = NULL;
    int *p2 = NULL;

    p1 = (int *)malloc(sizeof(int));
    if (p1 == NULL) goto cleanup;

    p2 = (int *)malloc(sizeof(int));
    if (p2 == NULL) goto cleanup;

    // ... 使用 p1, p2 ...

cleanup:
    free(p1);  // free(NULL) 是安全的
    free(p2);
    return 0;
}

// 技巧3：C++ 中使用智能指针（推荐！）
// std::unique_ptr<int> p = std::make_unique<int>(42);
// 不需要手动 delete，离开作用域自动释放
```

## 内存对齐

内存对齐是计算机为了提高访问效率而做的一种优化。现代CPU通常按照固定的字节数（如4字节、8字节）来读取内存，如果数据地址是对齐的，一次就能读完；如果不对齐，可能需要多次读取。

### 对齐规则

1. **基本类型对齐**：每种类型有自己的对齐要求，通常是自身的大小。`char` 对齐1字节，`short` 对齐2字节，`int` 对齐4字节，`double` 对齐8字节。
2. **结构体对齐**：结构体中每个成员都按照自己的对齐要求放置，结构体的整体对齐要求等于最大成员的对齐要求。
3. **默认对齐**：编译器默认的对齐值通常是8字节（64位系统）。

### 结构体对齐示例

```c
struct Example1 {
    char   a;   // 1字节，对齐1
    // 3字节填充
    int    b;   // 4字节，对齐4
    short  c;   // 2字节，对齐2
    // 2字节填充（结构体大小必须是最大对齐的倍数）
};
// 总大小：1 + 3(填充) + 4 + 2 + 2(填充) = 12字节
```

```
内存布局：

偏移:  0    1 2 3    4 5 6 7    8 9    10 11
      ┌────┬───────┬────────┬───────┬──────┐
      │ a  │ 填充  │   b    │   c   │ 填充 │
      └────┴───────┴────────┴───────┴──────┘
       1B    3B       4B       2B      2B
```

### 优化结构体布局

通过调整成员顺序，可以减少填充，节省内存：

```c
// 未优化：16字节
struct Bad {
    char   a;    // 1 + 7填充
    double b;    // 8
    char   c;    // 1 + 7填充
};

// 优化后：16字节
struct Good {
    double b;    // 8
    char   a;    // 1
    char   c;    // 1 + 6填充
};

// 更优：24字节 → 16字节
struct Better {
    double b;    // 8
    char   a;    // 1
    char   c;    // 1 + 6填充
};
```

**优化原则：** 按成员大小从大到小排列，可以最大程度减少填充。

### 修改对齐值

```c
// 使用 #pragma pack 修改对齐值
#pragma pack(1)   // 按1字节对齐（取消填充）
struct Packed {
    char   a;    // 1
    int    b;    // 4
    short  c;    // 2
};
// 总大小：7字节（没有填充）
#pragma pack()    // 恢复默认对齐

// C11 标准方式
struct alignas(1) Packed2 {
    char   a;
    int    b;
    short  c;
};
```

## 常见面试题

### 面试题1：栈和堆的区别？

**标准答案：**

> 见上面的对比表格。核心回答：栈由编译器自动管理，速度快但空间小；堆由程序员手动管理，空间大但速度慢。栈上的数据在函数返回后自动销毁，堆上的数据需要手动释放。

### 面试题2：malloc 的底层实现原理？

**标准答案：**

> malloc 的底层实现通常使用以下技术：
> 1. **空闲链表**：维护一个空闲内存块的链表，分配时从中找合适的块
> 2. **首次适应 / 最佳适应**：选择空闲块的策略
> 3. **brk/sbrk 和 mmap**：向操作系统申请内存的系统调用。小内存用 brk 扩展堆，大内存用 mmap 映射
> 4. **内存池**：glibc 的 ptmalloc 维护了多个内存池（arena），减少锁竞争
>
> 分配流程：先在对应大小的 bin 中查找，找不到就切割较大的块，还不够就向操作系统申请。

### 面试题3：什么是内存泄漏？怎么检测和避免？

**标准答案：**

> 内存泄漏是指程序分配了内存但未能正确释放，导致这块内存无法被再次使用。长期运行的服务程序如果存在内存泄漏，会逐渐耗尽可用内存。
>
> 检测方法：Valgrind、AddressSanitizer、Visual Studio 内存诊断。
>
> 避免方法：每个 malloc 都有对应的 free；使用 goto 统一清理；C++ 中优先使用智能指针；代码审查时重点检查动态内存分配。

### 面试题4：new/delete 和 malloc/free 的区别？

**标准答案：**

> 核心区别有三点：
> 1. new/delete 是运算符，malloc/free 是函数
> 2. new 会调用构造函数，delete 会调用析构函数，malloc/free 不会
> 3. new 失败抛异常，malloc 失败返回 NULL
>
> 所以在C++中，对象应该用 new/delete，基本类型两者都可以。但推荐统一使用 new/delete。

### 面试题5：什么是内存对齐？为什么要内存对齐？

**标准答案：**

> 内存对齐是指数据在内存中的起始地址是其大小的整数倍。比如 int（4字节）的地址应该是4的倍数。
>
> 原因：
> 1. **硬件效率**：CPU从对齐的地址读取数据更快，可能只需要一次内存访问
> 2. **平台兼容性**：某些硬件平台不支持访问未对齐的地址，会触发异常
> 3. **原子操作**：对齐的内存访问在某些架构上是原子的

## 踩坑提醒

### 坑1：malloc 不初始化

```c
int *p = (int *)malloc(sizeof(int));
printf("%d\n", *p);  // 输出随机值！应该用 calloc 或者手动赋值
```

### 坑2：double free

```c
int *p = (int *)malloc(sizeof(int));
free(p);
free(p);  // 灾难！同一块内存释放两次，未定义行为
```

### 坑3：free 栈上的内存

```c
int a = 10;
free(&a);  // 错误！a 在栈上，不能用 free
```

### 坑4：new 和 delete[] 不匹配

```cpp
int *p = new int[10];
delete p;      // 错误！应该用 delete[]
delete[] p;    // 正确
```

### 坑5：内存越界

```c
int *p = (int *)malloc(sizeof(int) * 5);
p[5] = 42;  // 越界！只有 p[0]~p[4] 是合法的
```

## 完整示例代码

所有示例代码可以在 [memory_demo.c](../../../code/01-c-basics/memory_demo.c) 中找到，编译运行：

```bash
gcc -Wall -o memory_demo memory_demo.c && ./memory_demo
```

---

> 上一节：[1.2 数组与字符串](../array/README.md) | 下一节：[1.4 struct与union](../struct/README.md)
