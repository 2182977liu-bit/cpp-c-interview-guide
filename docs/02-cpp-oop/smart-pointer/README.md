# 2.4 智能指针

> 作者：青松与桑叶
> C/C++ 面试通关指南 -- 第二部分：C++ 面向对象

---

## 目录

- [一、为什么需要智能指针](#一为什么需要智能指针)
- [二、RAII 思想](#二raii-思想)
- [三、std::unique_ptr —— 独占所有权](#三stdunique_ptr--独占所有权)
- [四、std::shared_ptr —— 共享所有权](#四stdshared_ptr--共享所有权)
- [五、std::weak_ptr —— 解决循环引用](#五stdweak_ptr--解决循环引用)
- [六、循环引用问题详解](#六循环引用问题详解)
- [七、面试真题](#七面试真题)

---

## 一、为什么需要智能指针

### 1.1 手动内存管理的痛点

在 C++ 中，使用 `new` 分配的内存必须手动 `delete` 释放。但实际开发中，手动管理内存非常容易出错：

```cpp
void problem() {
    int* p = new int(42);
    // ... 一堆代码 ...
    if (someCondition) {
        return;  // 忘记 delete！内存泄漏！
    }
    // ... 更多代码 ...
    delete p;
}
```

常见的内存管理问题：

```
┌──────────────────────────────────────────────────────────┐
│              手动内存管理的三大问题                         │
├──────────────────────────────────────────────────────────┤
│  1. 内存泄漏：new 了但忘记 delete                          │
│  2. 重复释放：同一块内存 delete 了两次（double free）       │
│  3. 悬空指针：delete 后继续使用指针（野指针）               │
└──────────────────────────────────────────────────────────┘
```

### 1.2 智能指针的登场

C++11 引入了智能指针，它可以**自动管理动态内存**，在适当的时候自动释放资源，从根本上解决上述问题。

```cpp
#include <memory>

void solution() {
    // 智能指针自动管理内存，不需要手动 delete
    auto p = make_unique<int>(42);
    // ... 任何情况下退出函数，内存都会被自动释放 ...
}
```

C++11 提供了三种智能指针，定义在 `<memory>` 头文件中：

| 智能指针 | 所有权模型 | 引用计数 | 头文件 |
|---------|-----------|---------|--------|
| `std::unique_ptr` | 独占所有权 | 无 | `<memory>` |
| `std::shared_ptr` | 共享所有权 | 有 | `<memory>` |
| `std::weak_ptr` | 不拥有所有权 | 无 | `<memory>` |

> **面试必答：** C++11 之后，请使用智能指针代替裸指针（`new`/`delete`），这是现代 C++ 的基本要求。

---

## 二、RAII 思想

### 2.1 什么是 RAII

RAII（Resource Acquisition Is Initialization，资源获取即初始化）是 C++ 中最重要的资源管理思想。

核心原则：**将资源的生命周期绑定到对象的生命周期**。

```
RAII 的工作原理：

  对象创建（构造函数）→ 获取资源
  对象使用中           → 持有资源
  对象销毁（析构函数）→ 自动释放资源
```

### 2.2 RAII 的示例

RAII 不仅仅用于内存管理，还可以管理任何资源：

```cpp
// 1. 内存管理
{
    vector<int> v;       // 构造时分配内存
    v.push_back(42);
    // 使用 v...
}  // 离开作用域，析构函数自动释放内存

// 2. 文件管理
{
    ifstream file("data.txt");  // 构造时打开文件
    // 读取文件...
}  // 离开作用域，析构函数自动关闭文件

// 3. 锁管理
{
    lock_guard<mutex> lock(mtx);  // 构造时加锁
    // 临界区操作...
}  // 离开作用域，析构函数自动解锁
```

### 2.3 为什么 RAII 能解决问题

RAII 利用了 C++ 的一个关键特性：**局部对象离开作用域时，析构函数一定会被调用**（即使是因为异常退出）。

```
不管函数怎么退出：
  - 正常 return  ✓ 析构函数被调用
  - 异常抛出    ✓ 析构函数被调用（栈展开）
  - break       ✓ 析构函数被调用
  - 任何其他方式 ✓ 析构函数被调用

所以资源一定会被释放！
```

---

## 三、std::unique_ptr —— 独占所有权

### 3.1 基本概念

`unique_ptr` 表示对一块内存的**独占所有权**。同一时刻，只能有一个 `unique_ptr` 指向某个对象。当 `unique_ptr` 被销毁时，它指向的对象也会被自动删除。

### 3.2 创建 unique_ptr

```cpp
#include <memory>

// 推荐方式：使用 make_unique（C++14）
auto p1 = make_unique<int>(42);       // 指向 int
auto p2 = make_unique<string>("hello"); // 指向 string
auto p3 = make_unique<Dog>("旺财");    // 指向自定义类

// 不推荐的方式（直接用 new）
unique_ptr<int> p4(new int(42));  // 可以但不推荐，异常不安全
```

> **为什么推荐 `make_unique`？**
> 1. 更安全：避免因参数求值顺序导致的内存泄漏
> 2. 代码更简洁
> 3. 性能可能更好（减少一次内存分配）

### 3.3 unique_ptr 的特性

```cpp
auto p1 = make_unique<int>(42);

// 1. 不能拷贝
// auto p2 = p1;  // 编译错误！unique_ptr 不可拷贝

// 2. 可以移动
auto p2 = std::move(p1);  // OK！p1 变为 nullptr，p2 接管所有权
// 此时 p1 为空，p2 指向原来的对象

// 3. 像裸指针一样使用
cout << *p2 << endl;    // 解引用
cout << p2.get() << endl; // 获取原始指针

// 4. 释放所有权（不删除对象）
int* raw = p2.release();  // p2 变为 nullptr，需要手动 delete raw

// 5. 重置
p2.reset();  // 删除当前对象，p2 变为 nullptr
p2.reset(new int(100));  // 删除当前对象，指向新对象
```

### 3.4 unique_ptr 管理数组

```cpp
// 管理动态数组
auto arr = make_unique<int[]>(10);
arr[0] = 42;
arr[1] = 100;
```

### 3.5 unique_ptr 在函数中的使用

```cpp
// 工厂函数：返回 unique_ptr（通过移动语义）
unique_ptr<Dog> createDog() {
    return make_unique<Dog>("旺财");  // 移动语义，无拷贝
}

// 接收 unique_ptr 参数
void process(unique_ptr<Dog> dog) {
    dog->bark();
}  // dog 离开作用域，自动释放

// 传递所有权
auto myDog = createDog();
process(std::move(myDog));  // 所有权转移给 process
// 此时 myDog 为 nullptr
```

---

## 四、std::shared_ptr —— 共享所有权

### 4.1 基本概念

`shared_ptr` 允许多个指针**共享**同一个对象的所有权。它通过**引用计数**来跟踪有多少个 `shared_ptr` 指向同一个对象。当最后一个 `shared_ptr` 被销毁时，对象才会被删除。

### 4.2 引用计数的工作原理

```
创建 shared_ptr：
  auto p1 = make_shared<int>(42);
  // 引用计数 = 1

拷贝 shared_ptr：
  auto p2 = p1;
  // 引用计数 = 2（p1 和 p2 共享同一个对象）

再拷贝：
  auto p3 = p2;
  // 引用计数 = 3

p3 离开作用域：
  // 引用计数 = 2

p2 离开作用域：
  // 引用计数 = 1

p1 离开作用域：
  // 引用计数 = 0 → 自动删除对象！
```

### 4.3 创建 shared_ptr

```cpp
// 推荐方式
auto p1 = make_shared<int>(42);

// 不推荐（两次内存分配）
shared_ptr<int> p2(new int(42));

// 查看引用计数
cout << p1.use_count() << endl;  // 输出：1

auto p3 = p1;
cout << p1.use_count() << endl;  // 输出：2
cout << p3.use_count() << endl;  // 输出：2
```

### 4.4 shared_ptr 的特性

```cpp
auto p1 = make_shared<string>("hello");

// 1. 可以拷贝
auto p2 = p1;  // OK！引用计数 +1

// 2. 可以移动
auto p3 = std::move(p1);  // p1 变为 nullptr，引用计数不变

// 3. 像裸指针一样使用
cout << *p2 << endl;
cout << p2->size() << endl;

// 4. 检查引用计数
cout << p2.use_count() << endl;  // 2（p2 和 p3）

// 5. 重置
p2.reset();  // 引用计数 -1
cout << p3.use_count() << endl;  // 1

// 6. 检查是否唯一持有
if (p3.unique()) {
    cout << "只有 p3 指向该对象" << endl;
}
```

### 4.5 make_shared 的优势

```
直接 new 的方式：
  1. new 分配对象内存
  2. new 分配控制块内存
  总共：两次内存分配

make_shared 的方式：
  1. 一次分配足够大的内存，同时存放对象和控制块
  总共：一次内存分配

优势：
  - 减少内存分配次数，提高性能
  - 内存局部性更好（对象和控制块在连续内存中）
  - 异常安全
```

---

## 五、std::weak_ptr —— 解决循环引用

### 5.1 为什么需要 weak_ptr

`shared_ptr` 有一个致命问题：**循环引用**。当两个对象互相持有对方的 `shared_ptr` 时，它们的引用计数永远不会变为 0，导致内存泄漏。

`weak_ptr` 就是为了解决这个问题而生的。它是对 `shared_ptr` 的**弱引用**，不会增加引用计数。

### 5.2 weak_ptr 的特性

```cpp
auto sp = make_shared<int>(42);

// 1. 从 shared_ptr 创建 weak_ptr
weak_ptr<int> wp = sp;
// sp.use_count() 仍然是 1（weak_ptr 不增加引用计数）

// 2. 检查所指向的对象是否还存在
if (!wp.expired()) {
    cout << "对象仍然存活" << endl;
}

// 3. 访问对象：必须先 lock() 提升为 shared_ptr
if (auto locked = wp.lock()) {
    cout << *locked << endl;  // 安全访问
} else {
    cout << "对象已被销毁" << endl;
}
```

### 5.3 weak_ptr 的使用场景

```
weak_ptr 的典型使用场景：

1. 解决 shared_ptr 的循环引用
2. 观察者模式中，观察者不希望影响被观察者的生命周期
3. 缓存系统，缓存不应该阻止对象被回收
4. 打破 parent-child 循环引用（child 持有 parent 的 weak_ptr）
```

---

## 六、循环引用问题详解

### 6.1 什么是循环引用

```
对象 A 持有 shared_ptr<B>（指向 B）
对象 B 持有 shared_ptr<A>（指向 A）

A 的引用计数 = 1（来自 B）
B 的引用计数 = 1（来自 A）

当 A 和 B 离开作用域时：
  A 的引用计数：1 → 0？不！B 还持有 A 的 shared_ptr
  B 的引用计数：1 → 0？不！A 还持有 B 的 shared_ptr

结果：两个对象都无法被释放 → 内存泄漏！
```

### 6.2 用代码演示循环引用

```cpp
#include <memory>
#include <iostream>
using namespace std;

class B;  // 前向声明

class A {
public:
    shared_ptr<B> bPtr;
    ~A() { cout << "A 析构" << endl; }
};

class B {
public:
    shared_ptr<A> aPtr;  // 循环引用！
    ~B() { cout << "B 析构" << endl; }
};

void test() {
    auto a = make_shared<A>();
    auto b = make_shared<B>();
    a->bPtr = b;
    b->aPtr = a;  // 循环引用！A 和 B 都不会被析构
}
// test() 结束后，a 和 b 的引用计数都是 2，不会降到 0
// A 和 B 的析构函数都不会被调用 → 内存泄漏！
```

### 6.3 用 weak_ptr 解决循环引用

```cpp
class B;

class A {
public:
    shared_ptr<B> bPtr;
    ~A() { cout << "A 析构" << endl; }
};

class B {
public:
    weak_ptr<A> aPtr;  // 改用 weak_ptr！
    ~B() { cout << "B 析构" << endl; }
};

void test() {
    auto a = make_shared<A>();
    auto b = make_shared<B>();
    a->bPtr = b;
    b->aPtr = a;  // weak_ptr 不增加引用计数
}
// test() 结束后：
// a 的引用计数 = 1（只有 bPtr 持有）→ 降到 0，A 被析构
// A 析构后，bPtr 被释放，b 的引用计数降到 0，B 被析构
// 输出：A 析构 → B 析构（顺序可能不同）
```

### 6.4 循环引用的常见场景

```
1. 双向链表：prev 和 next 都用 shared_ptr
2. 树结构：parent 和 children 互相引用
3. 观察者模式：被观察者和观察者互相引用
4. 缓存系统：缓存对象引用了原始对象

解决方案：
  - 确定谁"拥有"谁，只有拥有者使用 shared_ptr
  - 被引用方使用 weak_ptr
  - 例如：树结构中 parent 持有 children 的 shared_ptr，
    children 持有 parent 的 weak_ptr
```

---

## 七、面试真题

### 真题 1：智能指针有哪些？分别有什么特点？

**答：** C++11 提供了三种智能指针：
1. `unique_ptr`：独占所有权，不可拷贝只能移动，零开销抽象，是最轻量的智能指针
2. `shared_ptr`：共享所有权，内部使用引用计数，可以拷贝，当引用计数为 0 时自动释放
3. `weak_ptr`：弱引用，不增加引用计数，用于解决 `shared_ptr` 的循环引用问题，使用前需要 `lock()` 提升为 `shared_ptr`

### 真题 2：shared_ptr 的线程安全性如何？

**答：** `shared_ptr` 的引用计数操作是**原子操作**，因此多个线程可以安全地拷贝同一个 `shared_ptr`（引用计数的增减是线程安全的）。但是，通过 `shared_ptr` 访问所指向的对象**不是线程安全的**——如果多个线程同时读写同一个对象，仍然需要加锁。简单来说：控制块是线程安全的，数据不是。

### 真题 3：make_shared 和直接 new 有什么区别？

**答：** `make_shared` 只进行一次内存分配（对象和控制块在同一块内存中），而直接用 `new` 需要两次分配（分别分配对象和控制块）。`make_shared` 的优势：性能更好、内存局部性更好、异常安全。但 `make_shared` 也有一个缺点：对象和控制块在同一块内存中，只有当 `weak_ptr` 全部过期后才能释放整块内存，如果 `weak_ptr` 生命周期很长，可能导致内存无法及时回收。

### 真题 4：什么是循环引用？如何解决？

**答：** 循环引用是指两个或多个对象通过 `shared_ptr` 互相持有对方，导致引用计数永远无法降到 0，从而造成内存泄漏。解决方案是使用 `weak_ptr` 打破循环。`weak_ptr` 是对 `shared_ptr` 的弱引用，不增加引用计数，不会阻止对象被释放。使用时需要通过 `lock()` 方法提升为 `shared_ptr` 来安全访问对象。

### 真题 5：智能指针的线程安全性？

**答：** 详细说明：
- `unique_ptr` 和裸指针一样，不是线程安全的
- `shared_ptr` 的引用计数是线程安全的（使用原子操作），但访问指向的对象不是线程安全的
- `weak_ptr` 的 `lock()` 是线程安全的
- 如果多个线程需要共享修改同一个对象，仍然需要使用 `mutex` 等同步机制

### 真题 6：什么场景下用 unique_ptr，什么场景下用 shared_ptr？

**答：** 优先使用 `unique_ptr`，只有在确实需要共享所有权时才使用 `shared_ptr`。`unique_ptr` 零开销，语义清晰（独占所有权），可以隐式转换为 `shared_ptr`。`shared_ptr` 有引用计数的开销，而且可能引入循环引用问题。大部分场景下，通过移动语义传递 `unique_ptr` 就可以满足需求。

---

## 完整示例代码

本节所有概念的演示代码请参考：

[smart_pointer_demo.cpp](../../../code/02-cpp-oop/smart_pointer_demo.cpp)

编译运行：

```bash
g++ -std=c++17 -Wall -o smart_pointer_demo smart_pointer_demo.cpp
./smart_pointer_demo
```

---

## 上一节 | 下一节

[2.3 多态与虚函数](../polymorphism/README.md) | [2.5 虚函数深度剖析](../virtual/README.md)

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
