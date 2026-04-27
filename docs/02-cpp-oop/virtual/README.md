# 2.5 虚函数深度剖析

> 作者：青松与桑叶
> C/C++ 面试通关指南 -- 第二部分：C++ 面向对象

---

## 目录

- [一、虚函数表（vtable）的内存布局](#一虚函数表vtable的内存布局)
- [二、单继承下的 vtable](#二单继承下的-vtable)
- [三、多重继承下的 vtable（多个 vptr）](#三多重继承下的-vtable多个-vptr)
- [四、虚析构函数](#四虚析构函数)
- [五、虚函数的性能开销](#五虚函数的性能开销)
- [六、面试真题](#六面试真题)

---

## 一、虚函数表（vtable）的内存布局

### 1.1 vtable 是什么

当一个类中声明了虚函数时，编译器会为这个类生成一个**虚函数表（Virtual Function Table，简称 vtable）**。vtable 是一个静态数组，存储了该类所有虚函数的函数指针。

同时，该类的每个对象内部都会包含一个**虚函数表指针（vptr）**，指向该类的 vtable。

### 1.2 vtable 和 vptr 的关系

```
类级别（所有对象共享）：
  ┌─────────────────────────────────┐
  │         vtable（虚函数表）        │
  │  ┌───────────────────────────┐  │
  │  │ &Base::func1              │  │  ← 虚函数指针
  │  │ &Base::func2              │  │  ← 虚函数指针
  │  │ &Base::func3              │  │  ← 虚函数指针
  │  └───────────────────────────┘  │
  └─────────────────────────────────┘

对象级别（每个对象独立）：
  ┌─────────────────────────────────┐
  │  对象内存布局                     │
  │  ┌───────────────────────────┐  │
  │  │ vptr ──────────────────────┼──┼──→ 指向类的 vtable
  │  ├───────────────────────────┤  │
  │  │ 成员变量 1                  │  │
  │  │ 成员变量 2                  │  │
  │  │ ...                        │  │
  │  └───────────────────────────┘  │
  └─────────────────────────────────┘
```

**关键理解：**
- vtable 是**类级别**的：同一个类的所有对象共享同一个 vtable
- vptr 是**对象级别**的：每个对象都有自己的 vptr，但都指向同一个 vtable
- vtable 在**编译期**生成
- vptr 在**构造函数**中初始化

### 1.3 虚函数调用的过程

当我们通过基类指针调用虚函数时，实际发生了什么？

```
Base* p = new Derived();
p->func1();

调用过程：
  1. 通过指针 p 找到对象的内存
  2. 通过对象内存中的 vptr 找到 vtable
  3. 在 vtable 中查找 func1 对应的函数指针
  4. 通过函数指针调用实际的函数

  p -> [vptr] -> [vtable] -> [&Derived::func1] -> 调用 Derived::func1()
```

这就是动态绑定的底层实现：**一次额外的间接寻址**。

### 1.4 用 ASCII 图理解完整布局

```
  代码段                          数据段/只读段
  ┌──────────┐                  ┌──────────────────┐
  │ Base::   │                  │ Base 的 vtable    │
  │  func1() │◄─────────────────│  [0] &Base::func1 │
  │          │                  │  [1] &Base::func2 │
  │ Base::   │                  └──────────────────┘
  │  func2() │◄─────────────────┐
  │          │                  │ Derived 的 vtable  │
  │ Derived::│◄─────────────────│  [0] &Derived::func1│ ← 重写了！
  │  func1() │                  │  [1] &Base::func2   │ ← 继承的
  │          │                  └──────────────────┘
  │ Derived::│
  │  func3() │                  栈/堆
  │          │                  ┌──────────────────┐
  └──────────┘                  │ Base 对象         │
                                │  [vptr] ──────────┼──→ Base 的 vtable
                                │  [base_data]      │
                                └──────────────────┘
                                ┌──────────────────┐
                                │ Derived 对象      │
                                │  [vptr] ──────────┼──→ Derived 的 vtable
                                │  [base_data]      │
                                │  [derived_data]   │
                                └──────────────────┘
```

---

## 二、单继承下的 vtable

### 2.1 基本示例

```cpp
class Base {
public:
    virtual void func1() { cout << "Base::func1" << endl; }
    virtual void func2() { cout << "Base::func2" << endl; }
    int baseData = 10;
};

class Derived : public Base {
public:
    void func1() override { cout << "Derived::func1" << endl; }
    virtual void func3() { cout << "Derived::func3" << endl; }
    int derivedData = 20;
};
```

### 2.2 单继承的 vtable 布局

```
Base 的 vtable：
  ┌──────────────────────────┐
  │ [0] &Base::func1         │
  │ [1] &Base::func2         │
  └──────────────────────────┘

Derived 的 vtable：
  ┌──────────────────────────┐
  │ [0] &Derived::func1      │  ← 重写了 Base::func1
  │ [1] &Base::func2         │  ← 继承 Base::func2，未重写
  │ [2] &Derived::func3      │  ← 新增的虚函数
  └──────────────────────────┘

Base 对象的内存布局：
  ┌──────────────────────────┐
  │ vptr ──→ Base 的 vtable   │
  │ baseData (int)            │
  └──────────────────────────┘

Derived 对象的内存布局：
  ┌──────────────────────────┐
  │ vptr ──→ Derived 的 vtable│
  │ baseData (int)            │  ← 继承自 Base
  │ derivedData (int)         │  ← Derived 自己的
  └──────────────────────────┘
```

**关键点：**
1. 派生类的 vtable 中，重写的函数指针被替换为派生类的版本
2. 未重写的函数指针保持基类的版本
3. 派生类新增的虚函数追加在 vtable 末尾
4. vptr 始终在对象内存的起始位置（大多数编译器如此）

### 2.3 构造过程中的 vptr 变化

在构造派生类对象时，vptr 会经历变化：

```
构造 Derived 对象的过程：

  1. 调用 Base 构造函数
     → vptr 指向 Base 的 vtable
     → 此时如果调用虚函数，调用的是 Base 的版本

  2. 调用 Derived 构造函数
     → vptr 改为指向 Derived 的 vtable
     → 此时如果调用虚函数，调用的是 Derived 的版本

这就是为什么在基类构造函数中调用虚函数不会产生多态效果！
```

---

## 三、多重继承下的 vtable（多个 vptr）

### 3.1 多重继承的 vtable 布局

当派生类继承多个基类时，每个含有虚函数的基类都会有自己的 vptr：

```cpp
class Base1 {
public:
    virtual void func1() { cout << "Base1::func1" << endl; }
    virtual void func2() { cout << "Base1::func2" << endl; }
    int data1 = 1;
};

class Base2 {
public:
    virtual void func3() { cout << "Base2::func3" << endl; }
    int data2 = 2;
};

class Derived : public Base1, public Base2 {
public:
    void func1() override { cout << "Derived::func1" << endl; }
    void func3() override { cout << "Derived::func3" << endl; }
    int data3 = 3;
};
```

### 3.2 多重继承的内存布局

```
Derived 对象的内存布局：

  ┌──────────────────────────────┐
  │ Base1 子对象                  │
  │  ┌────────────────────────┐  │
  │  │ vptr1 ──→ vtable1      │  │  ← 指向 Base1 部分的 vtable
  │  │ data1 (int)            │  │
  │  └────────────────────────┘  │
  ├──────────────────────────────┤
  │ Base2 子对象                  │
  │  ┌────────────────────────┐  │
  │  │ vptr2 ──→ vtable2      │  │  ← 指向 Base2 部分的 vtable
  │  │ data2 (int)            │  │
  │  └────────────────────────┘  │
  ├──────────────────────────────┤
  │ Derived 部分                  │
  │  data3 (int)                 │
  └──────────────────────────────┘
```

### 3.3 多重继承的 vtable 内容

```
vtable1（Base1 部分）：
  ┌──────────────────────────┐
  │ [0] &Derived::func1      │  ← 重写了 Base1::func1
  │ [1] &Base1::func2        │  ← 继承自 Base1
  └──────────────────────────┘

vtable2（Base2 部分）：
  ┌──────────────────────────┐
  │ [0] &Derived::func3      │  ← 重写了 Base2::func3
  └──────────────────────────┘
```

### 3.4 指针转换时的地址调整

多重继承中，基类指针指向派生类对象时，可能需要调整地址：

```cpp
Derived d;
Base1* p1 = &d;   // p1 == &d（地址不变）
Base2* p2 = &d;   // p2 == &d + sizeof(Base1)（地址偏移！）
```

```
Derived 对象：
  地址 0x100: [vptr1] [data1]    ← Base1 部分，p1 指向这里
  地址 0x108: [vptr2] [data2]    ← Base2 部分，p2 指向这里
  地址 0x110: [data3]            ← Derived 部分

  p1 = &d        → 0x100
  p2 = &d        → 0x108（编译器自动偏移）
  (Derived*)p2   → 0x100（编译器自动偏移回来）
```

> **面试高频：** 多重继承中，将派生类指针赋给不同基类指针时，地址可能不同。这是因为不同基类子对象在派生类对象中的位置不同。编译器会自动进行地址调整（thunk）。

---

## 四、虚析构函数

### 4.1 为什么需要虚析构函数

当通过基类指针删除派生类对象时，如果基类的析构函数不是虚函数，只会调用基类的析构函数，派生类的析构函数不会被调用，导致资源泄漏。

```cpp
class Base {
public:
    ~Base() { cout << "Base 析构" << endl; }  // 不是虚函数！
};

class Derived : public Base {
public:
    ~Derived() { cout << "Derived 析构" << endl; }
};

Base* p = new Derived();
delete p;  // 只调用 Base::~Base()！Derived::~Derived() 不会被调用！
```

### 4.2 虚析构函数的 vtable 表现

```
非虚析构函数时：
  Base 的 vtable：
    [0] &Base::func1
    （没有析构函数条目）

  delete p 时：直接调用 Base::~Base()

虚析构函数时：
  Base 的 vtable：
    [0] &Base::func1
    [1] &Base::~Base()         ← 析构函数在 vtable 中

  Derived 的 vtable：
    [0] &Derived::func1
    [1] &Derived::~Derived()   ← 派生类的析构函数

  delete p 时：通过 vtable 调用 Derived::~Derived()
    → 先执行 Derived 析构函数体
    → 再自动调用 Base 析构函数
```

### 4.3 虚析构函数的调用顺序

```cpp
delete p;  // p 是 Base*，实际指向 Derived 对象

// 实际调用顺序：
// 1. Derived::~Derived() 的函数体
// 2. Base::~Base() 的函数体
// 3. 释放内存
```

### 4.4 纯虚析构函数

有时候基类是抽象类，但你又希望它有虚析构函数。这时可以使用**纯虚析构函数**：

```cpp
class Base {
public:
    virtual ~Base() = 0;  // 纯虚析构函数
};

// 纯虚析构函数必须提供实现！
Base::~Base() {
    cout << "Base 析构" << endl;
}
```

**为什么纯虚析构函数需要实现？** 因为在析构派生类对象时，基类的析构函数总是会被调用。如果基类析构函数没有实现，链接时会报错。

---

## 五、虚函数的性能开销

### 5.1 虚函数的开销来源

```
虚函数的性能开销：

  1. 空间开销
     - 每个对象多一个 vptr（通常 8 字节，64 位系统）
     - 每个类多一个 vtable（每个虚函数一个指针，通常 8 字节）
     - 多重继承时可能有多个 vptr

  2. 时间开销
     - 每次虚函数调用需要一次额外的间接寻址（查 vtable）
     - 可能影响 CPU 分支预测和指令缓存
     - 编译器更难进行内联优化

  3. 间接开销
     - 虚函数调用通常不能被内联（除了去虚化优化）
     - 影响编译器优化空间
```

### 5.2 开销有多大

```
虚函数调用 vs 普通函数调用：

  普通函数调用：
    call func_addr        ← 直接跳转，1 条指令

  虚函数调用：
    mov rax, [obj]        ← 取 vptr
    mov rax, [rax+offset] ← 查 vtable
    call rax              ← 间接调用

  额外开销：约 2-3 条指令 + 一次内存访问
  在现代 CPU 上，这通常只有几个纳秒的差距
```

### 5.3 什么时候需要考虑虚函数的开销

```
需要考虑的场景：
  - 性能极其敏感的代码（游戏引擎、高频交易、信号处理）
  - 在 tight loop 中大量调用虚函数
  - 嵌入式系统，内存极其有限
  - 实时系统，需要确定性的执行时间

不需要考虑的场景：
  - 大多数业务逻辑代码
  - 虚函数调用不在性能热点上
  - 开发效率和可维护性更重要

优化建议：
  - 如果确定不需要多态，就不要用虚函数
  - 使用 final 关键字帮助编译器去虚化
  - 使用 CRTP（Curiously Recurring Template Pattern）实现静态多态
  - 考虑使用 std::variant（C++17）替代简单的多态层次
```

### 5.4 编译器的去虚化优化

现代编译器可以进行**去虚化（Devirtualization）**优化：

```cpp
class Dog : public Animal {
public:
    void speak() override { cout << "汪汪汪" << endl; }
};

Dog dog;
dog.speak();  // 编译器知道 dog 的确切类型，可以内联！
```

当编译器能确定对象的实际类型时，就可以跳过 vtable 查找，直接调用目标函数，甚至进行内联优化。

---

## 六、面试真题

### 真题 1：虚函数的实现机制？

**答：** 编译器为每个含有虚函数的类创建一个虚函数表（vtable），表中按顺序存储了该类所有虚函数的地址。每个对象内部有一个指向其类 vtable 的指针（vptr），通常放在对象内存的起始位置。通过基类指针调用虚函数时，运行时先通过对象的 vptr 找到 vtable，再从 vtable 中取出对应的函数指针进行调用。这就是动态绑定的底层实现。

### 真题 2：多重继承下有几个 vptr？

**答：** 如果派生类的多个基类都有虚函数（即都有自己的 vtable），那么派生类对象中会有多个 vptr，每个有虚函数的基类对应一个。如果某个基类没有虚函数，则不会有对应的 vptr。多重继承下，将派生类指针赋给不同基类指针时，地址可能不同（编译器自动偏移），因为不同基类子对象在派生类对象中的位置不同。

### 真题 3：构造函数中调用虚函数会怎样？

**答：** 在基类构造函数中调用虚函数时，调用的是基类自己的版本，不会产生多态效果。因为在构造基类部分时，派生类部分还没有被构造，此时 vptr 指向基类的 vtable。同理，在析构函数中调用虚函数也只会调用当前类（正在析构的类）的版本。

### 真题 4：虚函数表存在哪里？

**答：** 虚函数表通常存在进程的只读数据段（.rodata）。vtable 在编译期生成，是类级别的，同一类的所有对象共享同一个 vtable。vptr 存在对象的内存中，在构造函数中被初始化。

### 真题 5：虚函数的性能开销有哪些？如何优化？

**答：** 虚函数的开销包括：
1. 空间开销：每个对象多一个 vptr（8 字节），每个类多一个 vtable
2. 时间开销：每次调用需要间接寻址（查 vtable），约 2-3 条额外指令
3. 优化障碍：虚函数调用难以内联，影响编译器优化

优化方法：
- 不需要多态时不要用虚函数
- 使用 `final` 关键字帮助编译器去虚化
- 使用 CRTP 实现静态多态
- 使用 `std::variant`（C++17）替代简单的多态层次

### 真题 6：纯虚析构函数为什么需要实现？

**答：** 因为在析构派生类对象时，基类的析构函数总是会被自动调用（析构顺序是从派生类到基类）。如果纯虚析构函数没有实现，链接时会报错。纯虚析构函数是唯一一个必须提供实现的纯虚函数。

---

## 完整示例代码

本节所有概念的演示代码请参考：

[virtual_demo.cpp](../../../code/02-cpp-oop/virtual_demo.cpp)

编译运行：

```bash
g++ -std=c++17 -Wall -o virtual_demo virtual_demo.cpp
./virtual_demo
```

---

## 上一节

[2.4 智能指针](../smart-pointer/README.md)

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
