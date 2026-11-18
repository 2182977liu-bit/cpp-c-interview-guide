# 3.2 模板编程

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

---

## 目录

- 一、模板到底是什么：编译器帮你批量造代码
- 二、函数模板与实参推导：最常见的翻车点都在这
- 三、类模板：成员函数是“懒”的
- 四、特化与偏特化：只给特定类型开小灶
- 五、变参模板与折叠表达式
- 六、SFINAE 与 enable_if：让模板“只在合适的时候存在”
- 七、C++17 if constexpr 与 C++20 Concepts
- 八、两阶段名字查找：typename 和 this-> 为什么不能省
- 九、模板代码膨胀与编译期成本
- 十、面试真题
- 十一、踩坑提醒

---

## 一、模板到底是什么：编译器帮你批量造代码

先把一句话记牢：**编译器不会“编译”模板，它编译的是模板的实例化结果。**

你写 `template<typename T> T max(T a, T b)` 时，编译器只是把这东西记在小本本上，**一行机器码都不会生成**。直到你写 `max(1, 2)`，它才拿 `T = int` 真正造出一个函数。所以模板常被说成“代码模具”：模具本身不占地方，铸出来的件才占。

C++ 里有四种模板：

| 种类 | 长什么样 | 出现版本 |
|---|---|---|
| 函数模板 | `template<typename T> void f(T)` | C++98 |
| 类模板 | `template<typename T> class Box` | C++98 |
| 变量模板 | `template<typename T> constexpr T pi = ...` | C++14 |
| 别名模板 | `template<typename T> using Vec = std::vector<T>` | C++11 |

两个必须分清的概念：

- **实例化（instantiation）**：编译器用具体类型把模板“填实”，生成真正的代码；
- **特化（specialization）**：你亲手给某个类型写一份定制版，告诉编译器“这种类型别用模具，用我这份”。

> 面试点：模板是**编译期**机制，运行期零开销（除了它本身引入的间接调用）。这是 C++ 泛型区别于 Java 泛型擦除的关键：Java 的 `List<Integer>` 运行时还是 `List`，C++ 的 `vector<int>` 是真正的另一个类型。

---

## 二、函数模板与实参推导：最常见的翻车点都在这

### 2.1 推导不是“看你想什么”，而是“看形参长什么样”

```cpp
// deduce_demo.cpp —— g++ -std=c++17 -Wall -o deduce_demo deduce_demo.cpp
#include <cstdio>
#include <type_traits>

template<typename T>
void by_value(T v) {                       // 形参是值：会丢掉顶层 const 和数组/函数退化
    static_assert(std::is_same_v<T, int>, "T 被推导为 int");
    (void)v;
}

template<typename T>
void by_ref(T& v) {                        // 形参是引用：保留 const，不退化
    (void)v;
}

template<typename T>
void by_forward(T&& v) {                   // 万能引用：左值推成 T&，右值推成 T
    (void)v;
}

int main() {
    const int ci = 1;
    int arr[3] = {1, 2, 3};

    by_value(ci);            // T = int           （顶层 const 被去掉）
    by_value(arr);           // T = int*          （数组退化为指针）

    by_ref(ci);              // T = const int     （引用保留 const）

    int x = 0;
    by_forward(x);           // T = int&          （左值 → 引用折叠）
    by_forward(42);          // T = int           （右值 → 值）

    std::printf("all assertions passed\n");
    return 0;
}
```

这段代码把三条规则全演示了：

1. **值形参**：顶层 `const`、数组、函数都会退化（数组变指针、函数变函数指针）；
2. **引用形参**：不退化，`const` 保留；
3. **`T&&` 万能引用**：配合引用折叠，左值推成 `T&`，右值推成 `T`——`std::forward` 就是靠这个工作的。

### 2.2 两个参数推不出同一个 T

```cpp
max(1, 2.5);   // 错：T 一会儿是 int 一会儿是 double，推导冲突
```

解法有三种，面试官往往追问“你为什么选这种”：

```cpp
template<typename T, typename U>
auto max2(T a, U b) { return a < b ? b : a; }   // 方案一：两个模板参数，返回值 auto

template<typename T>
T max3(T a, T b) { return a < b ? b : a; }
max3<double>(1, 2.5);                          // 方案二：显式指定 T，让 1 隐式转换

// 方案三：C++20 的 std::common_type / std::common_reference 做类型归一
```

### 2.3 显式指定可以“短路”推导

`max3<double>(1, 2.5)` 里，`1` 会被转成 `double`。注意这是**普通函数调用的隐式转换**，不是模板推导——推导一旦被显式参数覆盖，就不再参与。

---

## 三、类模板：成员函数是“懒”的

类模板被实例化时，**只有真正用到的成员函数才会被实例化**。没调用的成员函数只要语法上合法，就不会生成代码，也不会因为“里面的类型操作不成立”而报错。

```cpp
template<typename T>
struct Wrapper {
    void ok()   { T t{}; (void)t; }        // 用到了，会实例化
    void boom() { T::undefined(); }        // 没调用就不会报错
};

Wrapper<int> w;   // 只实例化 ok()，boom() 不生成，编译通过
// w.boom();      // 一旦调用，才会在实例化点报错
```

这条规则解释了一个常见现象：**模板类里写了一个当前类型不支持的操作，只要不调用就不报错**，所以模板的错误往往“延迟”到你用某个成员函数时才爆出来。

静态成员同理：`template<typename T> struct S { static int count; };` 每个 `S<T>` 都有自己独立的一份 `count`。

---

## 四、特化与偏特化：只给特定类型开小灶

### 4.1 全特化 vs 偏特化

```cpp
// 主模板：什么类型都返回 unknown
template<typename T>
struct TypeName { static const char* get() { return "unknown"; } };

// 全特化：所有参数都写死
template<>
struct TypeName<int> { static const char* get() { return "int"; } };

// 偏特化：只固定一部分，剩下的还是模板参数
template<typename T>
struct TypeName<T*> { static const char* get() { return "pointer"; } };

template<typename T>
struct TypeName<std::vector<T>> { static const char* get() { return "vector"; } };
```

编译期选谁，规则是“**越具体越优先**”：`TypeName<int*>` 会选 `T*` 那个偏特化，而不是主模板；`TypeName<int>` 选全特化。

### 4.2 函数模板不能偏特化——这是必考题

标准只允许**类模板**（和 C++14 的变量模板）做偏特化，**函数模板只能全特化**。

```cpp
// 你想写的“偏特化”，在函数模板上是不合法的
template<typename T> void f(T* p);       // 这是重载，不是偏特化
template<typename T> void f(T  p);
```

上面两个能共存，是因为它们是**两个不同的主模板重载**，而不是“主模板 + 偏特化”。面试时把这句话说清楚就够了：函数模板靠**重载 + 全特化 + if constexpr / Concepts** 实现条件化，偏特化只属于类模板。

### 4.3 特化和重载谁优先

在同样能匹配的情况下，重载决议的顺序大致是：**非模板函数 > 更特化的模板 > 更泛化的模板**。所以“写一个同名普通函数”往往能压过模板，这也是很多库用普通函数做“兜底覆盖”的原因。

---

## 五、变参模板与折叠表达式

C++11 的变参模板配合递归展开很笨重，C++17 的折叠表达式把这件事变成一行：

```cpp
#include <iostream>

// C++17 右折叠：a + (b + (c + ...))
template<typename... Args>
auto sum(Args... args) { return (args + ...); }

// 逗号折叠：对每个参数执行一次函数调用
template<typename F, typename... Args>
void for_each_arg(F f, Args&&... args) { (f(std::forward<Args>(args)), ...); }

int main() {
    std::cout << sum(1, 2, 3, 4) << "\n";            // 10
    for_each_arg([](int v){ std::cout << v << ' '; }, 7, 8, 9);  // 7 8 9
}
```

两个细节：

- `sizeof...(Args)` 可以在编译期拿到参数个数；
- 折叠表达式必须加括号，`(args + ...)` 和 `(args + ... + 0)` 是不同语义（后者带初始值，空包也合法）。

---

## 六、SFINAE 与 enable_if：让模板“只在合适的时候存在”

SFINAE = **S**ubstitution **F**ailure **I**s **N**ot **A**n **E**rror：模板实参替换失败时，编译器**不报错**，只是把这个候选从重载集合里悄悄拿掉。

```cpp
#include <type_traits>
#include <string>

// 只接受整数：返回值上用 enable_if
// 非整数时 enable_if 没有 ::type，替换失败 → 该重载被丢弃 → 编译期“不存在”
template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
twice(T v) { return v + v; }

// 用 void_t 检测类型有没有 size() 成员
template<typename T, typename = void>
struct has_size : std::false_type {};

template<typename T>
struct has_size<T, std::void_t<decltype(std::declval<T>().size())>> : std::true_type {};

static_assert(has_size<std::string>::value, "string 有 size");
static_assert(!has_size<int>::value, "int 没有 size");

int main() { return twice(21) == 42 ? 0 : 1; }
```

三个必须知道的点：

1. **SFINAE 只作用于“立即上下文”**：替换过程中直接出现的类型/表达式失败才算；函数体内部报错不算 SFINAE，那是硬错误；
2. `enable_if` 可以放在**返回值**、**模板参数默认值**、**函数参数**三个位置，位置不同会影响重载的可读性与冲突概率；
3. `std::void_t` 是 C++17 的“检测任意成员/表达式是否存在”的万能工具，比手写 `decltype` 序列好读得多。

---

## 七、C++17 if constexpr 与 C++20 Concepts

### 7.1 if constexpr：编译期分支，不进代码

```cpp
template<typename T>
std::string describe(T v) {
    if constexpr (std::is_pointer_v<T>) {
        return v ? "pointer(non-null)" : "pointer(null)";   // 只有 T 是指针时这段才参与编译
    } else {
        return "value";
    }
}
```

关键差别：普通 `if` 的两个分支**都要编译通过**；`if constexpr` 的未选中分支**直接丢弃**，所以可以写“只对某些类型合法”的代码。

### 7.2 Concepts：把 SFINAE 换成人话

```cpp
#include <concepts>
#include <iostream>

// C++20：约束直接写在模板头，错误信息能指出“不满足 integral”
template<std::integral T>
T twice(T v) { return v + v; }

// requires 子句：更复杂的约束条件
template<typename T>
    requires requires(T t) { t.size(); }
std::size_t len(const T& t) { return t.size(); }

int main() {
    std::cout << twice(21) << "\n";        // 42
    std::cout << len(std::string("abc")) << "\n";
}
```

面试时这么对比最清楚：

| 维度 | SFINAE / enable_if | Concepts（C++20） |
|---|---|---|
| 可读性 | 模板头一堆 `enable_if`，难读 | 一行 `std::integral T` |
| 错误信息 | 替换失败的连环报错 | 直接说“T 不满足 integral” |
| 重载约束 | 靠返回值/默认参数“藏”约束 | 约束是签名的一部分，可参与重载 |
| 兼容性 | C++11 起通用 | 需 C++20 编译器 |

一句话总结：**Concepts 不是新能力，是把 SFINAE 的“黑话”翻译成了人能看懂的话**。

---

## 八、两阶段名字查找：typename 和 this-> 为什么不能省

模板里的名字分两类，查找时机不同：

| 名字类型 | 查找时机 | 典型例子 |
|---|---|---|
| 非依赖名 | 模板**定义点** | `int x = 0;`、`foo()`（无模板参数参与） |
| 依赖名 | 模板**实例化点**（含 ADL） | `T::value`、`obj.method()`、`this->bar()` |

### 8.1 经典翻车：派生类模板调基类成员

```cpp
template<typename T>
struct Base { void bar() {} };

template<typename T>
struct Derived : Base<T> {
    void foo() { bar(); }        // 错：bar 是非依赖名，定义点找不到（基类还没确定）
};

template<typename T>
struct Derived2 : Base<T> {
    void foo() { this->bar(); }  // 对：this 依赖 T，bar 变成依赖名，实例化时再查
    void baz() { Base<T>::bar(); }  // 也可以
};
```

这就是“**模板基类的成员必须用 `this->` 或 `Base<T>::` 限定**”的底层原因，不是编译器欺负你，是两阶段查找的必然结果。

### 8.2 typename 什么时候必须写

```cpp
template<typename T>
void f() {
    T::Type* p;              // 编译器不知道 T::Type 是类型还是静态成员，可能按“乘法”解析
    typename T::Type* p2;    // 明确告诉编译器：这是类型
}
```

规则：**依赖类型名前面必须加 `typename`**，除非它已经在“只能是类型”的位置（如基类列表、函数参数类型）。C++20 起很多场景放宽了，但面试答旧规则更稳。

同理还有 `template` 关键字：`obj.template foo<int>()`，用于告诉编译器 `foo` 是模板而不是成员变量。

---

## 九、模板代码膨胀与编译期成本

每个不同的实参组合都会生成一份独立代码，`vector<int>`、`vector<double>`、`vector<MyType>` 各一份。类型越多，二进制越大、编译越慢——这就是**模板代码膨胀（code bloat）**。

缓解手段：

| 手段 | 做法 | 效果 |
|---|---|---|
| 显式实例化 | `template class std::vector<int>;` | 把生成集中到一个 TU |
| extern template | `extern template class std::vector<int>;` | 其他 TU 不再重复生成 |
| 类型擦除 | 把公共逻辑抽到非模板基类/函数 | 多类型共享一份实现 |
| 减少组合 | 用 `std::variant`/指针基类代替大量模板组合 | 压低实例化数量 |

编译期成本还有一个常被忽略的点：**模板的错误信息长度**。这是 Concepts 被发明出来的现实动机之一——不是功能需要，是可维护性需要。

---

## 十、面试真题

### 真题 1：模板在什么阶段生效？运行时有开销吗？

模板在编译期实例化，生成的是普通函数/类，运行期没有“查模板”的动作，所以没有额外开销；它带来的是编译时间变长和二进制变大，不是运行变慢。

### 真题 2：函数模板为什么不能偏特化？

标准只允许类模板（和变量模板）偏特化。函数模板的条件化要靠重载、全特化和 `if constexpr` / Concepts；写成 `template<typename T> void f(T*)` 与 `template<typename T> void f(T)` 是两个重载，不是偏特化。

### 真题 3：全特化和偏特化有什么区别？

全特化把所有模板参数都写死（`TypeName<int>`），偏特化只固定一部分、保留模板参数（`TypeName<T*>`）；匹配时越具体越优先。

### 真题 4：SFINAE 是什么？为什么叫“不是错误”？

模板实参替换失败时，如果失败发生在重载决议的立即上下文，编译器把这个候选从重载集合里移除而不报错，这就是 Substitution Failure Is Not An Error；函数体内部报错不属于 SFINAE。

### 真题 5：enable_if 一般放在哪几个位置？

三个位置：返回值类型、模板参数的默认值、函数参数。放返回值最直观但会影响返回类型推导，放模板参数默认值最常用（不污染签名），放函数参数适合需要参与重载的情况。

### 真题 6：C++20 Concepts 比 SFINAE 好在哪？

约束写在模板头、可读；不满足约束时错误信息直接指出哪一个 concept 没过，而不是一长串替换失败；约束成为签名的一部分，重载决议更清晰。

### 真题 7：模板基类的成员函数为什么要加 this->？

因为两阶段查找：不加限定的 `bar()` 是非依赖名，在模板定义点就要解析，而基类此时依赖 T、还没确定，于是找不到；加 `this->` 或 `Base<T>::` 把它变成依赖名，推迟到实例化点再查。

### 真题 8：typename 什么时候必须写？

在依赖类型名前必须写，例如 `typename T::value_type`，否则编译器可能把 `T::Type* p` 解析成乘法；在基类列表、函数参数等“只能是类型”的位置可以省。

### 真题 9：模板代码膨胀怎么解决？

显式实例化把生成集中、`extern template` 阻止其他 TU 重复生成、把公共逻辑抽成非模板实现、用 `std::variant` 或基类指针减少类型组合。

### 真题 10：`template<typename T> void f(T&&)` 里的 T 到底推成什么？

这是万能引用：传左值时 T 推成 `T&`，再经引用折叠得到 `T&`；传右值时 T 推成 `T`，形参是 `T&&`。这也是 `std::forward<T>` 能保留值类别的原理。

---

## 十一、踩坑提醒

1. **模板定义最好放头文件**：编译器在实例化点需要看到完整定义，只声明不定义会链接报错（除非显式实例化）。
2. **`>>` 在 C++11 前会被当成右移**：老代码里 `vector<vector<int> >` 的空格不是审美，是语法；C++11 起已放宽。
3. **`if constexpr` 里不能写“只对某类型合法”的代码在普通 if 里**：普通 if 的两个分支都要编译通过。
4. **特化必须在使用前可见**：如果某个 TU 先用主模板实例化了，再看到特化，标准不保证它换用特化（IFNDR，编译器可能直接不报错地出错）。
5. **别用 `enable_if` 堆成俄罗斯套娃**：两个以上条件时优先 `if constexpr` 或 Concepts，否则错误信息会失控。

---

## 参考资料

本章为原创整理，知识点覆盖对照了以下公开资料，未复制原文：

- cppreference：Templates / partial_specialization / SFINAE / constraints：<https://en.cppreference.com/w/cpp/language/templates>
- C++ 标准 [temp] 相关章节（两阶段查找、特化规则）
- Microsoft Learn《模板 (C++)》：<https://learn.microsoft.com/zh-cn/cpp/cpp/templates-cpp>
- bigcpp.com Templates / Two-Phase Name Lookup 参考页（用于对照考点覆盖范围）
- 《C++ Templates: The Complete Guide》（Vandevoorde 等）第 2、5、8 章
- 卡码笔记 C++ 面试题清单：<https://notes.kamacoder.com/cpp/>（仅作知识点覆盖对照）

---

## 上一节 | 下一节

[3.1 STL 容器与算法](../stl/README.md) | 3.3 右值引用与移动语义（待写）

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
