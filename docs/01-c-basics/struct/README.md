# 1.4 struct 与 union

> 作者：青松与桑叶 | 结构体是C语言面向对象的基础，搞懂内存对齐是关键

---

## 目录

- [struct 基本用法](#struct-基本用法)
- [struct 内存对齐](#struct-内存对齐)
- [位域](#位域)
- [大小端](#大小端)
- [union 的用途](#union-的用途)
- [常见面试题](#常见面试题)
- [踩坑提醒](#踩坑提醒)
- [完整示例代码](#完整示例代码)

---

## struct 基本用法

结构体（struct）是C语言中用来将不同类型的数据组合在一起的方式。可以理解为"自定义数据类型"。

```c
// 定义一个学生结构体
struct Student {
    char name[20];   // 姓名
    int age;         // 年龄
    float score;     // 成绩
};

// 使用
struct Student s1 = {"张三", 20, 95.5f};
struct Student s2;
s2.age = 21;

// 用 typedef 简化（推荐）
typedef struct Student {
    char name[20];
    int age;
    float score;
} Student;

Student s3 = {"李四", 22, 88.0f};  // 不用再写 struct 了
```

### 结构体指针

```c
Student s = {"王五", 20, 90.0f};
Student *p = &s;

// 两种访问方式
printf("%s\n", (*p).name);  // 解引用后用 .
printf("%s\n", p->name);    // 用 -> 更方便（推荐）
printf("%d\n", p->age);
```

`->` 是"箭头运算符"，专门用于通过指针访问结构体成员，等价于 `(*p).member`。

### 结构体作为函数参数

```c
// 值传递（会拷贝整个结构体，效率低）
void print_student(Student s) {
    printf("姓名: %s, 年龄: %d, 成绩: %.1f\n", s.name, s.age, s.score);
}

// 指针传递（只传地址，效率高）
void print_student_ptr(const Student *s) {
    printf("姓名: %s, 年龄: %d, 成绩: %.1f\n", s->name, s->age, s->score);
}

// 调用
Student s = {"赵六", 21, 92.5f};
print_student(s);         // 值传递
print_student_ptr(&s);    // 指针传递（推荐）
```

### 结构体内存分配

结构体占用的内存大小不一定等于所有成员大小之和，因为**内存对齐**的存在。

## struct 内存对齐

内存对齐是结构体面试中的核心考点。编译器会在成员之间插入"填充字节"，使得每个成员的地址满足其对齐要求。

### 对齐规则

1. **第一个成员**放在偏移量为 0 的位置
2. **每个成员**的偏移量必须是 `min(成员大小, 默认对齐值)` 的整数倍
3. **结构体总大小**必须是最大成员大小（或默认对齐值）的整数倍

### 示例分析

```c
struct Example1 {
    char   a;    // 1字节
    int    b;    // 4字节
    short  c;    // 2字节
};
```

```
分析过程：

成员 a (char, 1字节):
  偏移 0，对齐1，0 % 1 == 0 ✓
  占用: [0]

成员 b (int, 4字节):
  偏移 1，对齐4，1 % 4 != 0 ✗
  偏移 4，对齐4，4 % 4 == 0 ✓
  填充: [1,2,3]
  占用: [4,5,6,7]

成员 c (short, 2字节):
  偏移 8，对齐2，8 % 2 == 0 ✓
  占用: [8,9]

结构体总大小: 10字节
最大成员对齐: 4字节
10 % 4 != 0，需要填充到 12

最终布局:
偏移:  0    1 2 3    4 5 6 7    8 9    10 11
      ┌────┬───────┬────────┬───────┬──────┐
      │ a  │ 填充  │   b    │   c   │ 填充 │
      └────┴───────┴────────┴───────┴──────┘
       1B    3B       4B       2B      2B

sizeof(Example1) = 12
```

### 再来一个例子

```c
struct Example2 {
    char   a;     // 1字节
    char   b;     // 1字节
    int    c;     // 4字节
    short  d;     // 2字节
};
```

```
偏移:  0    1    2 3    4 5 6 7    8 9    10 11
      ┌────┬────┬───────┬────────┬───────┬──────┐
      │ a  │ b  │ 填充  │   c    │   d   │ 填充 │
      └────┴────┴───────┴────────┴───────┴──────┘
       1B   1B    2B       4B       2B      2B

sizeof(Example2) = 12
```

注意：a 和 b 连在一起，不需要填充，因为 char 的对齐是1。

### 含数组和结构体的嵌套

```c
struct Inner {
    char c;    // 1 + 3填充
    int i;     // 4
};             // 总共 8 字节

struct Outer {
    char a;        // 1 + 3填充
    struct Inner b; // 8字节（Inner 的对齐是4，因为最大成员是 int）
    short c;       // 2 + 2填充
};                 // 总共 16 字节
```

### 修改默认对齐

```c
// 方法1：#pragma pack
#pragma pack(1)    // 1字节对齐，取消所有填充
struct Packed {
    char a;
    int b;
    short c;
};  // 大小: 7字节
#pragma pack()     // 恢复默认

// 方法2：__attribute__（GCC）
struct __attribute__((packed)) Packed2 {
    char a;
    int b;
    short c;
};  // 大小: 7字节

// 方法3：指定对齐值
#pragma pack(2)    // 2字节对齐
struct Align2 {
    char a;    // 1 + 1填充
    int b;     // 4
    short c;   // 2
};  // 大小: 8字节
#pragma pack()
```

## 位域

位域（bit field）允许我们指定结构体成员占用的位数，常用于节省内存（比如网络协议头、硬件寄存器等场景）。

### 基本语法

```c
struct Flags {
    unsigned int readable   : 1;  // 占1位
    unsigned int writable   : 1;  // 占1位
    unsigned int executable : 1;  // 占1位
    unsigned int reserved   : 29; // 占29位
};
```

```
内存布局（假设32位系统）：

31                                  3  2  1  0
┌──────────────────────────────────┬───┬───┬───┐
│           reserved (29位)         │ e │ w │ r │
└──────────────────────────────────┴───┴───┴───┘

sizeof(Flags) = 4字节（32位）
```

### 使用示例

```c
struct Flags f;
f.readable = 1;
f.writable = 1;
f.executable = 0;

if (f.readable) {
    printf("文件可读\n");
}
```

### 位域的注意事项

1. 位域的类型通常是 `unsigned int` 或 `int`
2. 位域不能取地址（`&f.readable` 是非法的）
3. 位域的位数不能超过类型的位数（`unsigned int` 最多32位）
4. 跨类型的位域可能产生填充

## 大小端

大小端（Endianness）是指多字节数据在内存中的存储顺序。

### 什么是大端和小端

```c
int a = 0x12345678;
```

```
大端模式（Big-Endian）：高位字节存放在低地址
地址:   0x100  0x101  0x102  0x103
       ┌──────┬──────┬──────┬──────┐
       │ 0x12 │ 0x34 │ 0x56 │ 0x78 │
       └──────┴──────┴──────┴──────┘
        高位 →                          ← 低位

小端模式（Little-Endian）：低位字节存放在低地址
地址:   0x100  0x101  0x102  0x103
       ┌──────┬──────┬──────┬──────┐
       │ 0x78 │ 0x56 │ 0x34 │ 0x12 │
       └──────┴──────┴──────┴──────┘
        低位 →                          ← 高位
```

**记忆方法：** 小端 = "小（低）的端（地址）放小的（低位）字节"

### 判断大小端

```c
int is_little_endian() {
    int a = 1;
    // 如果第一个字节是 1，说明低位存在低地址，即小端
    return *(char *)&a == 1;
}
```

### 哪些是大端，哪些是小端

- **小端**：x86/x64 架构（大多数个人电脑和服务器）
- **大端**：ARM（默认）、PowerPC、网络字节序（TCP/IP协议规定使用大端）
- **可配置**：ARM、RISC-V 等架构可以通过配置切换

### 为什么网络传输用大端

因为大端更符合人类的阅读习惯（高位在前），TCP/IP 协议规定网络字节序为大端。所以进行网络编程时，经常需要进行字节序转换：

```c
#include <arpa/inet.h>

uint32_t host_val = 0x12345678;
uint32_t net_val = htonl(host_val);  // 主机序 → 网络序
uint32_t back = ntohl(net_val);      // 网络序 → 主机序
```

## union 的用途

联合体（union）和结构体类似，但所有成员**共享同一块内存**。联合体的大小等于最大成员的大小。

### 基本用法

```c
union Data {
    int i;
    float f;
    char str[20];
};

union Data d;
d.i = 42;
printf("%d\n", d.i);   // 42

d.f = 3.14f;
printf("%d\n", d.i);   // 值被覆盖了！不再是42
printf("%f\n", d.f);   // 3.14
```

```
内存布局：

union Data 的所有成员共享同一块内存：

偏移:  0 1 2 3 4 5 ... 19
      ┌──────────────────┐
d.i:  │     int (4B)     │
      ├──────────────────┤
d.f:  │    float (4B)    │
      ├──────────────────┤
d.str:│   char[20] (20B) │
      └──────────────────┘

sizeof(Data) = 20（最大成员的大小）
```

### union 的典型用途

#### 用途1：判断大小端

```c
union {
    int i;
    char c;
} test;

test.i = 1;
if (test.c == 1) {
    printf("小端\n");
} else {
    printf("大端\n");
}
```

#### 用途2：类型转换（不依赖指针强转）

```c
union FloatConverter {
    float f;
    unsigned int u;
    unsigned char bytes[4];
};

FloatConverter fc;
fc.f = 3.14f;
printf("IEEE 754: 0x%08x\n", fc.u);
printf("字节: %02x %02x %02x %02x\n",
       fc.bytes[0], fc.bytes[1], fc.bytes[2], fc.bytes[3]);
```

#### 用途3：节省内存（变体类型）

```c
// 一个变量可能是 int、float 或 char*
struct Value {
    int type;           // 0=int, 1=float, 2=string
    union {
        int int_val;
        float float_val;
        char *str_val;
    } data;
};
```

## 常见面试题

### 面试题1：结构体为什么要内存对齐？

**标准答案：**

> 内存对齐主要有三个原因：
> 1. **硬件效率**：CPU 从对齐的地址读取数据更快。比如32位CPU一次读取4字节，如果地址是4的倍数，一次就能读完一个int；如果不对齐，可能需要两次读取再拼接
> 2. **平台兼容性**：某些硬件平台（如ARM）不支持访问未对齐的地址，会触发异常
> 3. **原子操作**：对齐的内存访问在某些架构上是原子的，不对齐则不是

### 面试题2：怎么计算结构体的大小？

**标准答案：**

> 结构体大小 = 所有成员大小之和 + 填充字节。计算步骤：
> 1. 第一个成员从偏移0开始
> 2. 每个成员的偏移量 = `min(成员大小, pack值)` 的整数倍，不够就填充
> 3. 结构体总大小 = 最大成员对齐值的整数倍，不够就尾部填充
>
> 举例：`struct { char a; int b; short c; }` 的大小是12字节（1+3填充+4+2+2填充）。

### 面试题3：什么是大小端？怎么判断？

**标准答案：**

> 大小端是指多字节数据在内存中的存储顺序。大端是高位字节存放在低地址，小端是低位字节存放在低地址。
>
> 判断方法：取一个int变量的地址，强转为 char*，看第一个字节。如果是低位字节（如 0x78），就是小端。
>
> x86/x64 是小端，网络字节序是大端。

### 面试题4：union 和 struct 的区别？

**标准答案：**

> struct 的每个成员有独立的内存空间，总大小约等于所有成员大小之和（加上对齐填充）。union 的所有成员共享同一块内存，总大小等于最大成员的大小。
>
> union 适用于"同一时刻只用一个成员"的场景，可以节省内存。

## 踩坑提醒

### 坑1：结构体比较不能用 ==

```c
Student s1 = {"张三", 20, 90.0f};
Student s2 = {"张三", 20, 90.0f};
if (s1 == s2) {  // 编译错误！结构体不能直接用 == 比较
    // ...
}
// 正确做法：逐个成员比较，或者用 memcmp
if (s1.age == s2.age && s1.score == s2.score && strcmp(s1.name, s2.name) == 0) {
    // ...
}
```

### 坑2：union 同时使用多个成员

```c
union Data d;
d.i = 42;
d.f = 3.14f;
printf("%d\n", d.i);  // 不是42！被 d.f 覆盖了
```

### 坑3：位域取地址

```c
struct Flags {
    unsigned int readable : 1;
};
struct Flags f;
&f.readable;  // 编译错误！位域成员不能取地址
```

## 完整示例代码

所有示例代码可以在 [struct_demo.c](../../../code/01-c-basics/struct_demo.c) 中找到，编译运行：

```bash
gcc -Wall -o struct_demo struct_demo.c && ./struct_demo
```

---

> 上一节：[1.3 内存管理](../memory/README.md) | 下一节：[1.5 预处理与编译链接](../string/README.md)
