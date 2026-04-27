# 2.3 多态与虚函数

> 作者：青松与桑叶
> C/C++ 面试通关指南 -- 第二部分：C++ 面向对象

---

## 目录

- [一、什么是多态](#一什么是多态)
- [二、虚函数的声明和使用](#二虚函数的声明和使用)
- [三、纯虚函数和抽象类](#三纯虚函数和抽象类)
- [四、动态绑定 vs 静态绑定](#四动态绑定-vs-静态绑定)
- [五、override 和 final 关键字](#五override-和-final-关键字)
- [六、面试真题](#六面试真题)

---

## 一、什么是多态

### 1.1 多态的概念

多态（Polymorphism）是面向对象编程的三大特性之一（封装、继承、多态）。它的字面意思是"多种形态"。

在 C++ 中，多态是指**同一接口，不同实现**。通过基类的指针或引用调用同名函数，根据实际对象的类型，执行不同的代码。

### 1.2 生活中的多态

想象一个遥控器（基类指针），它可以控制不同的设备：

```
遥控器按钮 "powerOn()"
    ├── 电视 → 打开电视
    ├── 空调 → 启动空调
    └── 音响 → 播放音乐
```

同一个按钮（接口），按下后不同的设备（对象）有不同的反应（实现），这就是多态。

### 1.3 C++ 中的两种多态

```
┌──────────────────────────────────────────────────────────┐
│                    C++ 的多态                             │
├────────────────────────┬─────────────────────────────────┤
│     静态多态            │     动态多态                    │
│  （编译期多态）          │  （运行期多态）                  │
├────────────────────────┼─────────────────────────────────┤
│  函数重载              │  虚函数                         │
│  模板                  │  虚函数重写（override）          │
│  运算符重载            │                                │
│                        │                                │
│  编译器在编译期确定     │  程序在运行期根据对象类型确定     │
│  调用哪个函数           │  调用哪个函数                   │
└────────────────────────┴─────────────────────────────────┘
```

本节重点讲解**动态多态**，也就是通过虚函数实现的多态。

### 1.4 多态的基本示例

```cpp
class Animal {
public:
    virtual void speak() {   // 虚函数
        cout << "动物发出声音" << endl;
    }
};

class Dog : public Animal {
public:
    void speak() override {  // 重写虚函数
        cout << "汪汪汪！" << endl;
    }
};

class Cat : public Animal {
public:
    void speak() override {
        cout << "喵喵喵！" << endl;
    }
};

// 同一个函数，不同的行为 —— 这就是多态！
void makeSpeak(Animal& animal) {
    animal.speak();  // 运行时根据实际类型调用
}

int main() {
    Dog dog;
    Cat cat;
    makeSpeak(dog);  // 输出：汪汪汪！
    makeSpeak(cat);  // 输出：喵喵喵！
}
```

---

## 二、虚函数的声明和使用

### 2.1 虚函数的声明

在成员函数前加 `virtual` 关键字：

```cpp
class Base {
public:
    virtual void func() {
        cout << "Base::func()" << endl;
    }
};
```

### 2.2 虚函数的重写（Override）

派生类中定义与基类虚函数**签名完全相同**的函数，就构成重写：

```cpp
class Derived : public Base {
public:
    void func() override {  // override 是 C++11 的关键字（推荐使用）
        cout << "Derived::func()" << endl;
    }
};
```

**重写的条件（必须全部满足）：**
1. 函数名相同
2. 参数列表完全相同
3. 返回类型相同（或协变返回类型）
4. 基类函数必须是虚函数

### 2.3 虚函数的调用机制

```cpp
Animal* p = new Dog();
p->speak();  // 调用的是 Dog::speak()，不是 Animal::speak()
```

**为什么？** 因为 `speak()` 是虚函数，程序在运行时查看 `p` 实际指向的对象类型（`Dog`），然后调用 `Dog` 的 `speak()`。这就是**动态绑定**。

### 2.4 虚函数的注意事项

**1. 构造函数不能是虚函数**（上一节已解释）

**2. 内联函数可以是虚函数，但编译器通常会忽略 inline**

**3. 静态成员函数不能是虚函数**（没有 this 指针）

**4. 友元函数不能是虚函数**（友元不是成员函数）

**5. 只有类的成员函数才能是虚函数**（全局函数不行）

---

## 三、纯虚函数和抽象类

### 3.1 纯虚函数

纯虚函数是在基类中声明的、没有实现的虚函数。语法是在函数声明后加 `= 0`：

```cpp
class Shape {
public:
    // 纯虚函数 —— 子类必须实现
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual void draw() const = 0;
};
```

### 3.2 抽象类

含有纯虚函数的类称为**抽象类**。抽象类的特点：

1. **不能实例化**：`Shape s;` 是错误的
2. **只能作为基类**被继承
3. 派生类**必须实现所有纯虚函数**，否则派生类也是抽象类

```cpp
class Circle : public Shape {
private:
    double radius;
public:
    Circle(double r) : radius(r) {}

    double area() const override {
        return 3.14159265 * radius * radius;
    }

    double perimeter() const override {
        return 2 * 3.14159265 * radius;
    }

    void draw() const override {
        cout << "画一个半径为 " << radius << " 的圆" << endl;
    }
};

class Rectangle : public Shape {
private:
    double width, height;
public:
    Rectangle(double w, double h) : width(w), height(h) {}

    double area() const override {
        return width * height;
    }

    double perimeter() const override {
        return 2 * (width + height);
    }

    void draw() const override {
        cout << "画一个 " << width << "x" << height << " 的矩形" << endl;
    }
};
```

### 3.3 抽象类的意义

抽象类定义了一个**接口规范**，规定了子类必须实现哪些方法。这是面向对象设计中非常重要的概念：

```
抽象类 = 接口规范
  ↓
所有子类都必须遵守这个规范
  ↓
调用方只需要知道基类接口，不需要知道具体子类
  ↓
实现了"针对接口编程，而不是针对实现编程"
```

### 3.4 纯虚函数可以有实现

C++ 允许纯虚函数有函数体，但子类仍然必须重写它才能实例化：

```cpp
class Base {
public:
    virtual void func() = 0;  // 纯虚函数
};

void Base::func() {  // 可以在类外提供默认实现
    cout << "Base 默认实现" << endl;
}

class Derived : public Base {
public:
    void func() override {
        Base::func();  // 可以调用基类的默认实现
        cout << "Derived 扩展实现" << endl;
    }
};
```

这种技巧在"提供默认实现但强制子类显式选择"的场景下很有用。

---

## 四、动态绑定 vs 静态绑定

### 4.1 什么是绑定

绑定（Binding）是指将函数调用与具体函数代码关联起来的过程。

### 4.2 静态绑定（编译期绑定）

**非虚函数**使用静态绑定，编译器在编译时就确定了调用哪个函数：

```cpp
class Animal {
public:
    void speak() {  // 非虚函数
        cout << "动物发出声音" << endl;
    }
};

class Dog : public Animal {
public:
    void speak() {
        cout << "汪汪汪！" << endl;
    }
};

Animal* p = new Dog();
p->speak();  // 输出：动物发出声音（静态绑定，看指针类型）
```

编译器看到 `p` 的类型是 `Animal*`，就直接调用 `Animal::speak()`，不管 `p` 实际指向什么。

### 4.3 动态绑定（运行期绑定）

**虚函数**使用动态绑定，程序在运行时根据对象的实际类型决定调用哪个函数：

```cpp
class Animal {
public:
    virtual void speak() {  // 虚函数
        cout << "动物发出声音" << endl;
    }
};

class Dog : public Animal {
public:
    void speak() override {
        cout << "汪汪汪！" << endl;
    }
};

Animal* p = new Dog();
p->speak();  // 输出：汪汪汪！（动态绑定，看实际对象类型）
```

### 4.4 对比总结

```
┌──────────────────────────────────────────────────────────┐
│              静态绑定 vs 动态绑定                         │
├──────────────┬───────────────────┬───────────────────────┤
│              │   静态绑定         │   动态绑定             │
├──────────────┼───────────────────┼───────────────────────┤
│ 函数类型     │ 普通成员函数       │ 虚函数                 │
│ 确定时机     │ 编译期             │ 运行期                 │
│ 判断依据     │ 指针/引用的类型    │ 实际对象的类型          │
│ 性能         │ 高（无额外开销）    │ 略低（查虚函数表）      │
│ 灵活性       │ 低                 │ 高                     │
└──────────────┴───────────────────┴───────────────────────┘
```

### 4.5 动态绑定的条件

要实现动态绑定，必须**同时满足**以下条件：
1. 基类函数必须是虚函数
2. 必须通过**基类的指针或引用**来调用
3. 派生类必须重写了该虚函数

```cpp
Dog dog;
dog.speak();          // 直接调用，静态绑定（虽然 speak 是虚函数）

Animal& ref = dog;
ref.speak();          // 通过基类引用，动态绑定 ✓

Animal* ptr = &dog;
ptr->speak();         // 通过基类指针，动态绑定 ✓
```

> **面试高频：** 虚函数的实现机制是什么？
>
> 答：编译器为每个含有虚函数的类生成一个虚函数表（vtable），表中存储了该类虚函数的地址。每个对象内部有一个指向 vtable 的指针（vptr）。当通过基类指针调用虚函数时，程序先通过 vptr 找到 vtable，再从 vtable 中找到实际应该调用的函数地址。这个过程发生在运行时，因此称为动态绑定。

---

## 五、override 和 final 关键字

### 5.1 override 关键字（C++11）

`override` 放在派生类成员函数声明的末尾，表示这个函数**重写**了基类的虚函数。它不是必须的，但**强烈推荐使用**，因为编译器会帮你检查是否真的重写了基类函数。

```cpp
class Base {
public:
    virtual void func(int x) {}
    virtual void hello() const {}
};

class Derived : public Base {
public:
    // 正确重写
    void func(int x) override { cout << "重写成功" << endl; }

    // 编译错误！参数不匹配，不是重写
    // void func(double x) override {}

    // 编译错误！基类没有这个虚函数
    // void world() override {}

    // 编译错误！缺少 const
    // void hello() override {}
};
```

**override 的好处：** 防止因为拼写错误、参数类型不匹配、缺少 const 等原因导致的"你以为重写了，实际没有重写"的 bug。

### 5.2 final 关键字（C++11）

`final` 有两种用法：

**1. 修饰类：禁止该类被继承**

```cpp
class FinalClass final {
    // ...
};

// 编译错误！FinalClass 不能被继承
// class Derived : public FinalClass {};
```

**2. 修饰虚函数：禁止该虚函数在派生类中被重写**

```cpp
class Base {
public:
    virtual void func() final {
        cout << "这个函数不能被重写" << endl;
    }
};

class Derived : public Base {
public:
    // 编译错误！func 被声明为 final
    // void func() override {}
};
```

### 5.3 override 和 final 的使用建议

```
建议：
  - 派生类重写虚函数时，始终使用 override
  - 不希望被继承的类，使用 final
  - 不希望被重写的虚函数，使用 final
  - 这两个关键字都是编译期检查，零运行时开销
```

---

## 六、面试真题

### 真题 1：什么是多态？C++ 中如何实现？

**答：** 多态是指同一接口可以有不同的实现方式。C++ 中主要通过虚函数实现运行期多态（动态多态）：在基类中将函数声明为 `virtual`，在派生类中重写该函数，通过基类指针或引用调用时，程序在运行时根据实际对象的类型决定调用哪个版本的函数。C++ 还支持编译期多态（静态多态），如函数重载和模板。

### 真题 2：虚函数和纯虚函数的区别？

**答：** 虚函数有默认实现，派生类可以选择重写也可以不重写。纯虚函数没有实现（`= 0`），含有纯虚函数的类是抽象类，不能实例化，派生类必须实现所有纯虚函数才能实例化。纯虚函数主要用于定义接口规范。

### 真题 3：构造函数中可以调用虚函数吗？

**答：** 可以调用，但不会产生多态效果。在基类构造函数中调用虚函数时，调用的是基类自己的版本，而不是派生类重写的版本。因为在构造基类部分时，派生类部分还没有被构造，对象的类型还是基类。同理，析构函数中调用虚函数也类似。

### 真题 4：虚函数表（vtable）是什么？

**答：** 虚函数表是编译器为每个含有虚函数的类创建的一个静态数组，表中存储了该类所有虚函数的地址。每个对象内部有一个指向其类的 vtable 的指针（vptr）。通过基类指针调用虚函数时，运行时通过 vptr 找到 vtable，再从 vtable 中取出正确的函数地址进行调用。同一类的所有对象共享同一个 vtable。

### 真题 5：什么情况下不应该使用虚函数？

**答：** 以下情况不适合使用虚函数：
1. 类不会被继承时
2. 不需要通过基类指针/引用调用派生类方法时
3. 对性能要求极高的场景（虚函数调用有一次间接寻址开销）
4. 嵌入式系统等资源受限环境
5. 构造函数和析构函数中（析构函数可以是虚的，但构造函数不能）

---

## 完整示例代码

本节所有概念的演示代码请参考：

[polymorphism_demo.cpp](../../../code/02-cpp-oop/polymorphism_demo.cpp)

编译运行：

```bash
g++ -std=c++17 -Wall -o polymorphism_demo polymorphism_demo.cpp
./polymorphism_demo
```

---

## 上一节 | 下一节

[2.2 继承与组合](../inheritance/README.md) | [2.4 智能指针](../smart-pointer/README.md)

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
