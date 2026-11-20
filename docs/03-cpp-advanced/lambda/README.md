# 3.4 Lambda 与函数对象

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

---

## 目录

- 一、先认识“可调用对象”
- 二、Lambda 基本语法
- 三、捕获列表：决定闭包拿走什么
- 四、mutable：修改的是副本，不是原变量
- 五、Lambda 的底层：编译器生成了一个函数对象
- 六、泛型 Lambda、初始化捕获与返回值
- 七、this 捕获与异步回调的生命周期
- 八、std::function：统一接口背后的类型擦除
- 九、函数指针、函数对象、Lambda、std::function 怎么选
- 十、面试真题
- 十一、踩坑提醒

---

## 一、先认识“可调用对象”

只要一个东西能放在 `f(...)` 这种调用表达式里，就可以把它理解为可调用对象。常见形式有普通函数、函数指针、重载了 `operator()` 的类对象，以及 Lambda。

函数对象（functor）不是新语法，它只是一个重载了函数调用运算符的类：

```cpp
// functor_demo.cpp
// 编译：g++ -std=c++17 -Wall functor_demo.cpp -o functor_demo
#include <algorithm>
#include <iostream>
#include <vector>

struct Multiply {
    int factor;

    // 对象可以像函数一样被调用，同时还能保存 factor 状态。
    int operator()(int value) const {
        return value * factor;
    }
};

int main() {
    std::vector<int> values{1, 2, 3, 4};
    Multiply times3{3}; // 函数对象内部保存了乘数 3

    std::transform(values.begin(), values.end(), values.begin(), times3);

    for (int value : values) {
        std::cout << value << ' '; // 3 6 9 12
    }
    std::cout << '\n';
}
```

Lambda 的价值是：不必为了一个很短的回调，专门跑到类外面定义一个类型。它把“行为”和“需要的状态”写在算法旁边，代码更紧凑。

---

## 二、Lambda 基本语法

```text
[捕获列表](参数列表) mutable noexcept -> 返回类型 {
    函数体
}
```

其中只有捕获列表和函数体是核心，其他部分按需出现。无参数、没有 `mutable` 等说明符时，参数括号也能省略。

```cpp
// lambda_basic.cpp
// 编译：g++ -std=c++17 -Wall lambda_basic.cpp -o lambda_basic
#include <algorithm>
#include <iostream>
#include <vector>

int main() {
    auto add = [](int a, int b) {
        return a + b;
    };
    std::cout << add(2, 3) << '\n'; // 5

    auto hello = [] {
        std::cout << "hello lambda\n";
    };
    hello();

    std::vector<int> values{3, -1, 4, -2};
    std::sort(values.begin(), values.end(), [](int a, int b) {
        // 按绝对值排序；比较器必须满足严格弱序。
        return std::abs(a) < std::abs(b);
    });

    for (int value : values) {
        std::cout << value << ' '; // -1 -2 3 4
    }
    std::cout << '\n';
}
```

Lambda 也经常被“定义后立即调用”，用来把一段复杂初始化包起来：

```cpp
const int result = [] {
    int base = 10;
    return base * 2 + 1;
}(); // 末尾这对括号表示立即调用
```

---

## 三、捕获列表：决定闭包拿走什么

“捕获”就是 Lambda 把外层作用域里的变量带进自己的闭包对象。可以复制一份，也可以保存对原对象的访问通道。

| 写法 | 含义 | 典型用途 |
|---|---|---|
| `[]` | 不捕获 | 纯计算、无捕获函数指针转换 |
| `[x]` | 按值捕获 x | 保存当时快照 |
| `[&x]` | 按引用捕获 x | 同步修改外部 x |
| `[=]` | 使用到的局部变量默认按值捕获 | 简短的同步回调 |
| `[&]` | 使用到的局部变量默认按引用捕获 | 生命周期明确的局部算法 |
| `[=, &total]` | 默认按值，total 按引用 | 累加结果，同时保护其他变量 |
| `[&, factor]` | 默认按引用，factor 按值 | 少量变量需要快照 |
| `[p = std::move(p)]` | 初始化捕获，把资源移入闭包 | `unique_ptr` 等只移动对象 |
| `[this]` | 捕获当前对象指针 | 成员函数里的回调 |
| `[*this]` | C++17 起复制当前对象 | 保存整个对象的快照 |

```cpp
// capture_demo.cpp
// 编译：g++ -std=c++17 -Wall capture_demo.cpp -o capture_demo
#include <iostream>

int main() {
    int value = 10;

    auto snapshot = [value] {
        return value;
    };
    auto live = [&value] {
        return value;
    };

    value = 99;
    std::cout << snapshot() << '\n'; // 10：创建闭包时复制
    std::cout << live() << '\n';     // 99：调用时访问原变量

    int total = 0;
    auto accumulate = [&total](int x) {
        total += x;
    };
    accumulate(5);
    accumulate(7);
    std::cout << total << '\n'; // 12
}
```

按值捕获的时机是**创建 Lambda 时**，不是每次调用时。按引用捕获不会延长原变量寿命，所以回调如果需要保存到以后执行，就要格外小心悬空引用。

工程上更推荐显式写 `[x, &total]`，因为读者一眼就能看出依赖；在异步代码里尤其要慎用 `[=]` 和 `[&]` 一把抓。

---

## 四、mutable：修改的是副本，不是原变量

Lambda 默认生成的 `operator()` 是 const 成员函数，因此不能直接修改按值捕获的成员。加上 `mutable` 后，可以修改闭包内部的副本。

```cpp
// mutable_demo.cpp
// 编译：g++ -std=c++17 -Wall mutable_demo.cpp -o mutable_demo
#include <iostream>

int main() {
    int count = 0;

    auto next = [count]() mutable {
        ++count;      // 修改闭包里的 count 副本
        return count;
    };

    std::cout << next() << '\n'; // 1
    std::cout << next() << '\n'; // 2：同一个闭包保留了状态
    std::cout << count << '\n';  // 0：外部变量没变化

    auto copied = next;
    std::cout << copied() << '\n'; // 3
    std::cout << next() << '\n';   // 3：两个闭包各自维护状态
}
```

不要记成“`mutable` 让 Lambda 能修改外部变量”。真正规则是：

- 按值捕获 + `mutable`：修改闭包自己的副本；
- 按引用捕获：本来就可以修改所指对象，不需要 `mutable`；
- `mutable` 改变的是闭包 `operator()` 的 const 属性。

---

## 五、Lambda 的底层：编译器生成了一个函数对象

看到下面这段 Lambda：

```cpp
int factor = 3;
auto scale = [factor](int x) {
    return x * factor;
};
```

可以把它近似理解成编译器生成了这样的类：

```cpp
class CompilerGeneratedClosure {
public:
    explicit CompilerGeneratedClosure(int value) : factor_(value) {}

    int operator()(int x) const {
        return x * factor_;
    }

private:
    int factor_;
};
```

真实类型名和成员布局由编译器决定，我们不能依赖。每个 Lambda 表达式都有一个独特的闭包类型，所以通常用 `auto` 接收，而不能凭空写出它的类型名。

### 5.1 无捕获 Lambda 可以转换成函数指针

```cpp
#include <iostream>

int main() {
    int (*plusOne)(int) = [](int x) {
        return x + 1;
    };
    std::cout << plusOne(4) << '\n'; // 5

    auto captured = [n = 1](int x) { return x + n; };
    // int (*fp)(int) = captured; // 错误：有状态，不能退化成普通函数指针
    std::cout << captured(4) << '\n';
}
```

普通函数指针没有地方保存捕获状态，所以只有无捕获 Lambda 才支持这项转换。

---

## 六、泛型 Lambda、初始化捕获与返回值

### 6.1 泛型 Lambda

C++14 起参数可以写 `auto`，相当于生成了模板化的 `operator()`：

```cpp
#include <iostream>
#include <string>

int main() {
    auto print = [](const auto& value) {
        std::cout << value << '\n';
    };

    print(42);
    print(3.14);
    print(std::string{"hello"});
}
```

它比固定参数类型更通用，也不会自动变成 `std::function`；传给模板算法时仍保留具体闭包类型。

### 6.2 初始化捕获

初始化捕获让闭包直接拥有表达式的结果，还能把只移动对象移进去：

```cpp
// init_capture.cpp
// 编译：g++ -std=c++17 -Wall init_capture.cpp -o init_capture
#include <iostream>
#include <memory>
#include <utility>

// 模板参数直接接收闭包，因此支持只移动闭包。
template <typename F>
void run_once(F task) {
    task();
}

int main() {
    auto data = std::make_unique<int>(42);

    auto task = [ptr = std::move(data)] {
        std::cout << *ptr << '\n'; // 只读取，不需要 mutable
    };

    run_once(std::move(task)); // task 含 unique_ptr，只能移动
}
```

也可以捕获一个计算结果，避免每次调用重复计算：

```cpp
std::string prefix = "user-";
auto make_name = [text = prefix + "1001"] {
    return text;
};
```

### 6.3 返回类型

多数时候让编译器推导即可；不同返回语句的类型无法统一，或者必须返回引用时，使用尾置返回类型写清楚：

```cpp
auto identity = [](int& x) -> int& {
    return x; // 明确返回引用，而不是按 auto 推导出 int 副本
};
```

---

## 七、this 捕获与异步回调的生命周期

在成员函数中访问成员变量，Lambda 需要的是当前对象。`[this]` 保存的是指针，不是对象副本；`[*this]` 才会复制整个当前对象。

```cpp
// this_capture.cpp
// 编译：g++ -std=c++17 -Wall this_capture.cpp -o this_capture
#include <iostream>

class Counter {
public:
    explicit Counter(int value) : value_(value) {}

    void add(int delta) { value_ += delta; }

    auto live_reader() {
        return [this] {
            return value_; // 通过原对象指针读取
        };
    }

    auto snapshot_reader() const {
        return [*this] {
            return value_; // 读取闭包中保存的对象副本
        };
    }

private:
    int value_;
};

int main() {
    Counter counter{7};
    auto live = counter.live_reader();
    auto snapshot = counter.snapshot_reader();

    counter.add(5);
    std::cout << live() << '\n';     // 12
    std::cout << snapshot() << '\n'; // 7
}
```

一个容易误解的点：在成员函数里写 `[=]`，如果 Lambda 使用了成员，捕获的通常是 `this` 指针，并不意味着整个对象已经复制。对象销毁后再执行回调，仍然会产生未定义行为。

异步任务、定时器、网络回调选择捕获方式时，先回答“回调执行时，被捕获对象还活着吗”。需要共享所有权时可考虑 `shared_ptr`，只想观察对象是否存活时可用 `weak_ptr`，并在回调里 `lock()` 检查。

---

## 八、std::function：统一接口背后的类型擦除

不同 Lambda 的闭包类型不同，下面两个对象不能简单声明成同一种 `auto` 类型：

```cpp
auto f1 = [](int x) { return x + 1; };
auto f2 = [n = 2](int x) { return x + n; };
```

如果接口需要统一保存“所有接收 int、返回 int 的可调用对象”，可以使用 `std::function<int(int)>`：

```cpp
// function_demo.cpp
// 编译：g++ -std=c++17 -Wall function_demo.cpp -o function_demo
#include <functional>
#include <iostream>
#include <vector>

int twice(int x) {
    return x * 2;
}

int main() {
    std::vector<std::function<int(int)>> handlers;
    handlers.push_back(twice);
    handlers.push_back([](int x) { return x + 1; });
    handlers.push_back([base = 10](int x) { return base + x; });

    for (const auto& handler : handlers) {
        std::cout << handler(5) << '\n'; // 10、6、15
    }

    std::function<void()> empty;
    if (!empty) {
        std::cout << "empty function\n"; // 直接调用会抛 bad_function_call
    }
}
```

### 8.1 类型擦除是什么

调用方只看到固定签名 `R(Args...)`，不必知道内部到底是函数指针、Lambda 还是 functor。`std::function` 在内部保存目标，并通过统一调用机制转发，这就是类型擦除带来的多态。

代价主要包括：

- 调用通常多一层间接跳转，内联和优化更困难；
- 构造或复制目标可能需要动态内存，是否分配取决于目标大小和实现的小对象优化，**不能说每次都会分配堆内存**；
- 复制 `std::function` 通常要复制内部目标；
- 目标必须满足可复制构造要求，含 `unique_ptr` 的只移动闭包不能直接存入。

空的 `std::function` 被调用会抛 `std::bad_function_call`，所以注册表、事件分发器里调用前应检查。

---

## 九、函数指针、函数对象、Lambda、std::function 怎么选

| 方案 | 能否保存状态 | 类型是否固定 | 性能与适用场景 |
|---|---:|---:|---|
| 普通函数/函数指针 | 否（自身不带状态） | 是 | C 接口、简单回调 |
| 自定义 functor | 是 | 每个类一种类型 | 逻辑复杂、需要复用和命名 |
| Lambda | 是 | 每个表达式一种闭包类型 | STL 算法、局部短回调，利于内联 |
| `std::function` | 是 | 统一为指定签名 | 回调表、事件系统、需要运行期替换目标 |

热路径中优先让编译器看到具体类型：

```cpp
#include <iostream>
#include <utility>

// 调用点保留了 F 的具体类型，编译器更容易内联 task()。
template <typename F>
int apply(F&& task, int value) {
    return std::forward<F>(task)(value);
}

int main() {
    auto add = [base = 7](int x) { return base + x; };
    std::cout << apply(add, 5) << '\n'; // 12
}
```

不要为了“现代化”把所有回调都改成 `std::function`。需要类型擦除时才付它的成本；短逻辑优先 Lambda，复杂且复用多的逻辑仍值得命名成普通函数或 functor。

---

## 十、面试真题

### 真题 1：Lambda 的本质是什么？

三句话答案：Lambda 表达式会生成一个具有独特类型的闭包对象。这个类型重载了 `operator()`，捕获的数据由闭包保存。它可以近似理解成编译器帮你写了一个匿名 functor。

### 真题 2：按值捕获和按引用捕获有什么区别？

三句话答案：按值捕获在创建闭包时复制对象，之后原变量变化不影响副本。按引用捕获访问原对象，可以修改它，也不自动复制大对象。按引用捕获不延长生命周期，延迟回调最容易因此悬空。

### 真题 3：mutable 有什么作用？

三句话答案：Lambda 的调用运算符默认是 const，所以不能修改按值捕获的成员。`mutable` 去掉这一限制，让闭包内部副本可以修改并保存状态。它不会让按值捕获变成对外部原变量的修改。

### 真题 4：为什么异步 Lambda 不建议按引用捕获局部变量？

三句话答案：局部变量离开作用域就销毁，而异步回调可能更晚执行。闭包中的引用不会延长原对象寿命，执行时会访问已经失效的对象。应按值捕获、移动资源，或用合适的智能指针明确所有权。

### 真题 5：[this] 和 [*this] 有什么区别？

三句话答案：`[this]` 只保存当前对象指针，调用时仍访问原对象。`[*this]` 从 C++17 起复制当前对象，闭包持有独立快照。异步场景选择前者前必须保证对象比回调活得更久。

### 真题 6：为什么有捕获的 Lambda 不能转成普通函数指针？

三句话答案：普通函数指针只指向代码，没有存储捕获状态的配套对象。无捕获 Lambda 不需要保存状态，所以标准允许它转换。带捕获闭包必须保留自身对象，不能退化成普通函数指针。

### 真题 7：std::function 为什么能保存不同类型的 Lambda？

三句话答案：它按指定函数签名提供统一包装，内部通过类型擦除保存和调用目标。调用者不需要知道目标的具体闭包类型。统一接口的代价是间接调用、潜在分配和复制成本。

### 真题 8：std::function 一定会发生堆分配吗？

三句话答案：不一定，标准库实现通常会对小目标做小对象优化。目标较大或实现策略不同，才可能需要动态存储。性能判断应基于具体实现和基准测试，不能背成“每次必分配”。

### 真题 9：泛型 Lambda 和模板函数有什么关系？

三句话答案：参数使用 `auto` 的 Lambda，其 `operator()` 相当于函数模板。每次用不同参数类型调用时，会实例化对应版本。它仍然保持具体闭包类型，不依赖 `std::function`。

### 真题 10：模板回调和 std::function 回调怎么选？

三句话答案：模板保留具体可调用类型，更利于内联，也支持只移动闭包。`std::function` 提供运行期统一类型，适合存储异构回调和事件表。性能敏感且不需要类型擦除时优先模板，需要统一存储时再选 `std::function`。

---

## 十一、踩坑提醒

1. **循环里保存按引用捕获的循环变量**：多个回调可能最后都访问同一个变量，应按值保存每轮快照。
2. **返回捕获局部变量引用的 Lambda**：函数返回后局部变量销毁，再调用就是未定义行为。
3. **认为 `[=]` 会复制整个当前对象**：成员函数中它可能只捕获 `this` 指针，需要快照时用 `[*this]`。
4. **把只移动闭包塞进 `std::function`**：`std::function` 要求目标可复制构造，初始化捕获 `unique_ptr` 后应使用模板等方案。
5. **直接调用空的 `std::function`**：会抛 `std::bad_function_call`，先检查布尔状态。
6. **比较器 Lambda 写成 `a <= b`**：`sort` 等算法需要严格弱序，相等元素应返回 false。
7. **过度使用 `[=]`、`[&]`**：隐式依赖太多会增加审查难度，重要回调优先显式列捕获。
8. **把 `std::function` 当成零成本抽象**：类型擦除有成本，热路径不要无理由使用。

---

## 参考资料

本章为原创整理，示例代码为独立编写；仅把以下公开资料用于核对术语、规则与高频考点，未复制原文：

- cppreference 中文：Lambda 表达式：https://cppreference.cn/w/cpp/language/lambda
- cppreference 中文：std::function：https://cppreference.cn/w/cpp/utility/functional/function
- Microsoft Learn：《C++ 中的 Lambda 表达式》：https://learn.microsoft.com/zh-cn/cpp/cpp/lambda-expressions-in-cpp?view=msvc-170
- pkglog：《C++14 Init Capture: Moving unique_ptr into Lambdas》：https://pkglog.com/en/blog/cpp-init-capture/

---

## 上一节 | 下一节

[3.3 右值引用与移动语义](../move-semantics/README.md) | 3.5 多线程编程（待写）

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
