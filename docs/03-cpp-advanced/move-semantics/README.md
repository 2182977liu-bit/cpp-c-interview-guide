# 3.3 右值引用与移动语义

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

---

## 目录

- 一、为什么需要移动语义：能交接，就别复印
- 二、先分清值类别：左值、纯右值、将亡值
- 三、右值引用：让编译器知道“这份资源可以拿走”
- 四、移动构造与移动赋值：真正搬资源的地方
- 五、std::move 不移动，它只做类型转换
- 六、万能引用与 std::forward：原样转发左值和右值
- 七、RVO、NRVO 与 return std::move 的反面教材
- 八、noexcept：为什么会影响 vector 扩容速度
- 九、移动语义不是万能加速药
- 十、面试真题
- 十一、踩坑提醒

---

## 一、为什么需要移动语义：能交接，就别复印

假设一个对象手里有一块 100 MB 的堆内存。传统拷贝要做三件事：再申请 100 MB、逐字节复制、让两个对象各管各的缓冲区。可是如果源对象本来就是临时对象，马上就得析构，这份“复印”明显浪费。

**移动语义做的事是资源交接**：新对象接过源对象的指针，源对象把指针清空。通常不复制缓冲区，只改几个指针和整数，所以大对象的移动往往是常数时间。

| 操作 | 复制构造 | 移动构造 |
|---|---|---|
| 典型做法 | 新申请内存并复制内容 | 接管源对象的资源指针 |
| 大缓冲区成本 | 通常随数据量增长 | 通常与数据量无关 |
| 操作后源对象 | 内容不变 | 有效，但具体内容通常不能预设 |
| 常见参数 | `const T&` | `T&&` |

注意：移动语义优化的是**拥有资源的对象**，例如 `string`、`vector`、文件句柄、智能指针。一个只有几个 `int` 的对象，复制和移动没有本质差别。

---

## 二、先分清值类别：左值、纯右值、将亡值

值类别描述的是**表达式**，不是变量类型。判断时可以抓住两个问题：表达式有没有明确的身份？它的资源能不能被重新利用？

| 类别 | 有身份 | 可移动 | 例子 |
|---|---:|---:|---|
| 左值 lvalue | 是 | 通常否 | 变量名 `s`、`*p`、`v[0]`、前置 `++i` |
| 纯右值 prvalue | 否 | 是 | `42`、`std::string{"abc"}`、按值返回的函数调用 |
| 将亡值 xvalue | 是 | 是 | `std::move(s)`、返回 `T&&` 的函数调用 |
| 泛左值 glvalue | 是 | — | 左值或将亡值 |
| 右值 rvalue | — | 是 | 纯右值或将亡值 |

最容易绕晕的一条是：

```cpp
#include <string>
#include <utility>

void demo() {
    std::string s = "abc";
    std::string&& r = std::move(s);

    // 变量 r 的类型确实是右值引用；
    // 但表达式 r 有名字、有身份，所以它是左值。
    // 想再次把它当右值用，要写 std::move(r)。
}
```

**“类型是右值引用”和“表达式是右值”是两回事。** 这条规则解释了为什么函数里的右值引用参数，转发时经常还要写 `std::move` 或 `std::forward`。

---

## 三、右值引用：让编译器知道“这份资源可以拿走”

普通左值引用 `T&` 不能绑定临时对象；右值引用 `T&&` 可以绑定右值。给函数分别提供两种重载，编译器就能根据实参的值类别选择版本。

```cpp
// overload_demo.cpp
// 编译：g++ -std=c++17 -Wall overload_demo.cpp -o overload_demo
#include <iostream>
#include <string>
#include <utility>

void inspect(const std::string& s) {
    std::cout << "左值或只读使用：" << s << "\n";
}

void inspect(std::string&& s) {
    // 参数 s 有名字，在函数体内是左值；这里只演示重载选择。
    std::cout << "右值，可以接管资源：" << s << "\n";
}

int main() {
    std::string name = "qing song";
    inspect(name);                    // 左值，进入 const& 版本
    inspect(std::string{"temporary"}); // 临时对象，进入 && 版本
    inspect(std::move(name));         // 显式转为 xvalue，进入 && 版本
}
```

`const T&` 也能接右值，但当它和非 const 的 `T&&` 同时存在时，右值会优先选择后者。移动构造、移动赋值正是利用这套重载规则被自动选中的。

---

## 四、移动构造与移动赋值：真正搬资源的地方

下面是一个完整的小例子。复制时深拷贝，移动时只交接指针；源指针被置空，析构才不会重复释放。

```cpp
// buffer_move.cpp
// 编译：g++ -std=c++17 -Wall buffer_move.cpp -o buffer_move
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <utility>

class Buffer {
public:
    explicit Buffer(std::size_t n = 0)
        : data_(n ? new int[n]{} : nullptr), size_(n) {}

    ~Buffer() { delete[] data_; }

    // 复制构造：复制内容，两个对象拥有各自独立的缓冲区。
    Buffer(const Buffer& other) : Buffer(other.size_) {
        std::copy(other.data_, other.data_ + other.size_, data_);
    }

    // 复制赋值：先复制到临时对象，再交换，异常时当前对象不受影响。
    Buffer& operator=(const Buffer& other) {
        if (this != &other) {
            Buffer temp(other);
            swap(temp);
        }
        return *this;
    }

    // 移动构造：接管指针，并把源对象恢复成安全的空状态。
    Buffer(Buffer&& other) noexcept
        : data_(std::exchange(other.data_, nullptr)),
          size_(std::exchange(other.size_, 0)) {}

    // 移动赋值：先释放自己的旧资源，再接管源资源，并防止自移动赋值。
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            delete[] data_;
            data_ = std::exchange(other.data_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }

    void swap(Buffer& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }

    std::size_t size() const { return size_; }

private:
    int* data_ = nullptr;
    std::size_t size_ = 0;
};

int main() {
    Buffer a(1000);
    Buffer b(a);              // 复制构造，复制 1000 个元素
    Buffer c(std::move(a));   // 移动构造，只交接缓冲区指针
    b = std::move(c);         // 移动赋值

    std::cout << "a.size = " << a.size() << "\n"; // 本类约定移动后为空
    std::cout << "b.size = " << b.size() << "\n"; // 1000
}
```

### 4.1 移动后对象是什么状态

标准库类型通常只承诺“**有效但未指定**”：可以析构、赋值，或调用不依赖具体值的操作，但不要假设它一定为空。自己写的类最好把源对象整理成明确的空状态，降低误用风险。

### 4.2 Rule of Zero 与 Rule of Five

- 成员已经能管好资源时，优先用 `string`、`vector`、`unique_ptr`，让编译器生成特殊成员函数，这就是 **Rule of Zero**；
- 自己直接管理裸指针、文件描述符等资源时，如果需要自定义析构或复制操作，通常要把复制构造、复制赋值、移动构造、移动赋值和析构一起想清楚，这就是 **Rule of Five**；
- 只写移动构造却忘记处理源指针，是双重释放的常见来源。

---

## 五、std::move 不移动，它只做类型转换

这是面试中最常见的一句话：`std::move` **没有任何搬运动作**，它把表达式无条件转换成右值引用（更准确地说是 xvalue），让后续重载有机会选择移动操作。

```cpp
#include <iostream>
#include <string>
#include <utility>

int main() {
    std::string a = "hello";

    std::string&& ref = std::move(a);
    // 这里只产生了一个引用，没有构造新字符串，也没有搬缓冲区。
    std::cout << "转换后：" << a << "\n";

    std::string b = std::move(a);
    // 这里调用了 string 的移动构造，真正交接资源的是这个构造函数。
    std::cout << "b：" << b << "\n";
    std::cout << "a 仍可安全析构或重新赋值\n";
    ref = "reset"; // 不改变 ref 是 a 的引用这一事实，不建议这样绕用。
}
```

可以把它理解成一张“资源可交接声明”，而不是搬运工。写 `std::move(x)` 以后，通常就不应再读取 `x` 的业务内容；还要用，就先给它重新赋值。

---

## 六、万能引用与 std::forward：原样转发左值和右值

假设你写一层包装函数，调用者传左值时应该继续传左值，传右值时应该保留移动机会。如果函数体内直接写参数名，它永远是左值，右值身份就丢了。

```cpp
// forward_demo.cpp
// 编译：g++ -std=c++17 -Wall forward_demo.cpp -o forward_demo
#include <iostream>
#include <string>
#include <utility>

void consume(std::string& s) {
    std::cout << "收到左值：" << s << "\n";
}

void consume(std::string&& s) {
    std::cout << "收到右值：" << s << "\n";
}

template <typename T>
void relay(T&& value) {
    // value 在函数体内有名字；forward 根据 T 恢复调用方的值类别。
    consume(std::forward<T>(value));
}

int main() {
    std::string text = "lvalue";
    relay(text);                    // T 推导为 string&，仍传左值
    relay(std::string{"rvalue"});  // T 推导为 string，仍传右值
}
```

这里的 `T&&` 是**转发引用**（也常叫万能引用），因为 `T` 需要推导，而且参数形态正好是 `T&&`。并不是所有 `T&&` 都是万能引用：

```cpp
void f(std::string&& s);       // 普通右值引用，只接右值

template <typename T>
void g(std::vector<T>&& v);    // 也是右值引用，不是万能引用

template <typename T>
void h(T&& value);             // 转发引用，可接左值和右值
```

转发依赖引用折叠，记一句就够：**只要折叠中出现左值引用，结果就是左值引用；两个都是右值引用，才得到右值引用。**

| 写法 | 是否保留原值类别 | 使用场景 |
|---|---:|---|
| `consume(value)` | 否，按左值处理 | 不需要转发时 |
| `consume(std::move(value))` | 否，强制当右值 | 确定放弃这个对象 |
| `consume(std::forward<T>(value))` | 是 | 转发引用参数继续向下传 |

---

## 七、RVO、NRVO 与 return std::move 的反面教材

返回值优化让对象直接构造在接收位置，连移动都省掉。

```cpp
#include <string>

std::string make_temporary() {
    return std::string{"hello"}; // C++17 保证消除这里的临时对象拷贝/移动
}

std::string make_named() {
    std::string result = "hello";
    return result; // NRVO 常见但不强制；未发生时还能尝试隐式移动
}

int main() {
    std::string a = make_temporary();
    std::string b = make_named();
}
```

两种优化别混：

| 场景 | 名称 | C++17 是否强制 |
|---|---|---:|
| `return T{...};` 返回临时纯右值 | RVO / 保证复制消除 | 是 |
| `T obj; return obj;` 返回命名局部对象 | NRVO | 否 |

不建议给普通局部返回值画蛇添足：

```cpp
std::string bad() {
    std::string result = "hello";
    return std::move(result); // 把返回表达式固定成右值，可能妨碍 NRVO
}
```

通常直接 `return result;` 最好：编译器先做 NRVO，做不了时再按规则尝试移动。也不要返回局部对象的引用或右值引用，那会造成悬空引用。

---

## 八、noexcept：为什么会影响 vector 扩容速度

`vector` 扩容时，要把旧缓冲区里的元素转移到新缓冲区。如果移动构造中途抛异常，前面一些元素可能已经被掏空，旧容器就难以恢复，强异常保证也就难做了。

因此常见策略是：

- 移动构造为 `noexcept`：放心移动，速度快；
- 移动可能抛异常、但类型可复制：优先复制，保留旧元素用于回滚；
- 移动可能抛异常、类型又不可复制：只能移动，异常安全保证会变弱。

`std::move_if_noexcept(x)` 就体现了这个选择：可安全移动时给右值引用，否则给 `const T&`。面试里说清“**noexcept 会影响容器扩容时选移动还是复制**”，比只说“noexcept 表示不抛异常”更有深度。

```cpp
#include <type_traits>
#include <utility>
#include <vector>

struct Item {
    std::vector<int> data;
    Item() = default;
    Item(const Item&) = default;
    Item(Item&& other) noexcept : data(std::move(other.data)) {}
};

// 编译期确认移动构造不会抛异常，容器扩容可以优先采用移动。
static_assert(std::is_nothrow_move_constructible_v<Item>);

int main() {
    std::vector<Item> items(2);
    items.reserve(100); // 预留空间，避免后续反复扩容
}
```

注意 `noexcept` 不是性能装饰：函数里真的抛出未捕获异常时会走 `std::terminate`，所以只有确认不会抛才标。

---

## 九、移动语义不是万能加速药

| 场景 | 是否值得移动 | 原因 |
|---|---:|---|
| 大 `string`、`vector`、缓冲区 | 值得 | 可避免深拷贝 |
| `int`、小型 POD 结构 | 基本无差别 | 本来就是复制几个数值 |
| `unique_ptr` | 必须移动 | 独占所有权，复制被删除 |
| `shared_ptr` | 视情况 | 移动省一次引用计数原子操作，但不是省掉整份对象 |
| 还要使用的源对象 | 不要移动 | 后续内容不可依赖 |
| `emplace_back(args...)` | 可进一步减少临时对象 | 在容器内直接构造 |

`push_back(std::move(obj))` 是“先有一个对象，再把它移进容器”；`emplace_back(构造参数...)` 是“直接在容器里造对象”。两者解决的是不同环节，不要背成任何场景下 emplace 都更快。

---

## 十、面试真题

### 真题 1：左值和右值怎么区分？

三句话答案：左值表达式有稳定身份，例如变量名和解引用。纯右值主要是临时计算结果，将亡值则有身份但资源允许被接管。不能简单用“能否放在等号左边”判断，const 左值就不能赋值。

### 真题 2：为什么有名字的右值引用还是左值？

三句话答案：值类别看表达式，不看变量声明里的 `&&`。有名字的参数可以被反复引用，直接按右值掏空会很危险，所以它的名字表达式仍是左值。需要再次移动时要显式写 `std::move` 或 `std::forward`。

### 真题 3：std::move 到底做了什么？

三句话答案：它只做一次到右值引用的类型转换，本身不搬任何资源。真正的资源交接发生在随后被选中的移动构造或移动赋值里。转换以后一般不应再依赖源对象的具体内容。

### 真题 4：移动后源对象还能用吗？

三句话答案：标准库通常保证它处于有效但未指定状态，可以析构或重新赋值。不能据此假设字符串一定为空、vector 的 size 一定为零。自己实现移动操作时，最好把源对象整理成明确的安全状态。

### 真题 5：std::forward 和 std::move 有什么区别？

三句话答案：`std::move` 无条件把表达式转成右值。`std::forward<T>` 根据模板参数有条件转换，让左值仍是左值、右值仍是右值。它主要和转发引用 `T&&` 搭配，用于包装函数和工厂函数。

### 真题 6：所有 T&& 都是万能引用吗？

三句话答案：不是，只有满足模板参数推导等条件的 `T&&` 才是转发引用。`std::vector<T>&&` 这种已经限定外层类型的参数仍是右值引用。判断关键是类型是否按转发规则推导，而不是看到两个 `&` 就下结论。

### 真题 7：RVO 和 NRVO 有什么区别？

三句话答案：RVO 通常指返回临时纯右值时的复制消除，C++17 对这类场景有保证。NRVO 是返回命名局部对象时的优化，仍不强制。NRVO 没发生时，符合规则的返回通常还能尝试隐式移动。

### 真题 8：为什么不推荐 return std::move(local)？

三句话答案：普通 `return local;` 给编译器保留了做 NRVO 的机会。写成 `std::move(local)` 会改变返回表达式的形态，可能让本可省略的移动真正发生。返回局部对象时直接写名字，也绝不要返回它的引用。

### 真题 9：移动构造为什么最好加 noexcept？

三句话答案：`vector` 扩容需要维持异常安全，移动不抛异常时才能放心转移旧元素。若移动可能抛异常且类型可复制，容器可能退回复制来保证回滚。加 `noexcept` 前必须确认操作真的不会向外抛异常。

### 真题 10：什么时候编译器不会自动生成移动操作？

三句话答案：用户声明复制构造、复制赋值或析构等函数，会影响移动成员的自动生成规则，不能一概假设“编译器全都会补”。有时右值初始化最终退回调用复制构造，功能正确但性能不同。资源类最好显式使用 `= default`、`= delete` 或按 Rule of Five 写清意图。

---

## 十一、踩坑提醒

1. **不要对还会使用的对象随便 `std::move`**：移动不是复制，源对象内容之后不能按原值理解。
2. **不要返回局部对象的 `T&` 或 `T&&`**：移动语义不能延长局部对象寿命，返回后就是悬空引用。
3. **移动赋值先考虑自赋值和旧资源释放**：`x = std::move(x)` 不应把对象弄成无法析构的状态。
4. **接管资源后记得清源源指针**：只复制指针不清空，两个对象析构时会重复释放。
5. **别把 `std::forward<T>` 写成 `std::forward(value)`**：它通常需要显式模板参数来恢复值类别。
6. **不要滥用移动语义优化小对象**：先理解资源所有权，再用性能测试证明收益。

---

## 参考资料

本章为原创整理，示例代码为独立编写；仅把以下公开资料用于核对术语、规则与高频考点，未复制原文：

- cppreference：Value categories：https://cppreference.dev/w/cpp/language/value_category
- cppreference 中文：复制消除（RVO / NRVO）：https://cppreference.cn/w/cpp/language/copy_elision
- Microsoft Learn：《引用声明符：&&》：https://learn.microsoft.com/zh-cn/cpp/cpp/rvalue-reference-declarator-amp-amp?view=msvc-170
- C++ Cheat Sheet：Move Semantics（特殊成员函数、移动后状态、完美转发）：https://cppcheatsheet.readthedocs.io/en/latest/notes/cpp/cpp_move.html
- bigcpp.com：Move Semantics（值类别与返回路径）：https://bigcpp.com/reference/language/move-semantics

---

## 上一节 | 下一节

[3.2 模板编程](../template/README.md) | 3.4 Lambda 与函数对象（待写）

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
