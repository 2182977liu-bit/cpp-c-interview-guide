# 2.2 继承与组合

> 作者：青松与桑叶
> C/C++ 面试通关指南 -- 第二部分：C++ 面向对象

---

## 目录

- [一、继承的基本概念](#一继承的基本概念)
- [二、继承的语法和类型](#二继承的语法和类型)
- [三、单继承](#三单继承)
- [四、多继承](#四多继承)
- [五、虚继承——解决菱形问题](#五虚继承解决菱形问题)
- [六、组合 vs 继承](#六组合-vs-继承)
- [七、面试真题](#七面试真题)

---

## 一、继承的基本概念

### 1.1 什么是继承

继承是面向对象编程中实现**代码复用**的重要机制。它允许我们定义一个新类（派生类/子类），从已有类（基类/父类）那里继承属性和行为。

用生活中的例子来理解：

```
动物（基类）
├── 哺乳动物（派生类）
│   ├── 狗
│   └── 猫
└── 鸟类（派生类）
    ├── 鹰
    └── 企鹅
```

狗继承了哺乳动物的所有特征（恒温、胎生），又添加了自己的特征（会汪汪叫、嗅觉灵敏）。

### 1.2 为什么要用继承

1. **代码复用**：子类直接使用父类已有的代码，不用重复编写
2. **多态的基础**：通过基类指针/引用调用派生类的方法
3. **建立类之间的关系模型**：体现"is-a"关系（狗"是一个"哺乳动物）

---

## 二、继承的语法和类型

### 2.1 基本语法

```cpp
class 派生类 : 继承方式 基类 {
    // ...
};
```

### 2.2 三种继承方式

C++ 有三种继承方式，它们决定了基类成员在派生类中的访问权限：

#### public 继承（最常用）

```cpp
class Base {
public:
    int pub = 1;
protected:
    int prot = 2;
private:
    int priv = 3;
};

class Derived : public Base {
public:
    void test() {
        cout << pub;   // OK：public -> public
        cout << prot;  // OK：protected -> protected
        // cout << priv;  // 错误：private 不可访问
    }
};

int main() {
    Derived d;
    cout << d.pub;    // OK
    // cout << d.prot; // 错误：外部不可访问 protected
    // cout << d.priv; // 错误：外部不可访问 private
}
```

**访问权限变化表：**

| 基类成员 | public 继承 | protected 继承 | private 继承 |
|---------|------------|----------------|-------------|
| public | public | protected | private |
| protected | protected | protected | private |
| private | 不可访问 | 不可访问 | 不可访问 |

### 2.3 三种继承方式的区别

```
public 继承：
  "is-a" 关系 —— 派生类"是一种"基类
  外部代码可以访问基类的 public 成员
  最常用，符合面向对象设计原则

protected 继承：
  "has-access-to" 关系 —— 派生类可以访问基类的 public/protected
  外部代码不能访问基类的任何成员
  很少使用

private 继承：
  "implemented-in-terms-of" 关系 —— 派生类用基类来实现功能
  外部代码和子类都不能访问基类的任何成员
  通常可以用"组合"替代
```

> **面试怎么答：**
>
> **面试官：C++ 有哪几种继承方式？区别是什么？**
>
> 答：C++ 有 public、protected、private 三种继承方式。public 继承是最常用的，它保持了基类成员的访问级别不变，体现"is-a"关系。protected 继承将基类的 public 和 protected 成员都变为派生类的 protected。private 继承将所有可访问成员都变为 private。实际开发中几乎只用 public 继承，protected 和 private 继承可以用组合来替代。

---

## 三、单继承

### 3.1 基本示例

```cpp
class Animal {
protected:
    string name;
    int age;
public:
    Animal(string n, int a) : name(n), age(a) {}
    void eat() {
        cout << name << " 在吃东西" << endl;
    }
};

class Dog : public Animal {
private:
    string breed;
public:
    Dog(string n, int a, string b) : Animal(n, a), breed(b) {}
    void bark() {
        cout << name << " 汪汪叫" << endl;
    }
};
```

### 3.2 构造函数的调用顺序

创建派生类对象时，构造函数的调用顺序是：

```
1. 基类构造函数（先构造"父"部分）
2. 成员对象的构造函数（按声明顺序）
3. 派生类构造函数（最后构造"子"部分）
```

析构顺序则完全相反：

```
1. 派生类析构函数
2. 成员对象的析构函数
3. 基类析构函数
```

### 3.3 向上转型

派生类对象可以赋值给基类引用或指针（向上转型是安全的）：

```cpp
Dog dog("旺财", 3, "金毛");
Animal& ref = dog;     // OK：向上转型
Animal* ptr = &dog;    // OK：向上转型

ref.eat();    // OK
// ref.bark(); // 错误：基类引用不知道 bark 的存在
```

---

## 四、多继承

### 4.1 什么是多继承

一个类可以同时继承多个基类：

```cpp
class Teacher {
public:
    void teach() { cout << "老师在讲课" << endl; }
};

class Researcher {
public:
    void research() { cout << "在做研究" << endl; }
};

// 多继承：同时继承 Teacher 和 Researcher
class Professor : public Teacher, public Researcher {
public:
    void supervise() { cout << "在指导学生" << endl; }
};
```

### 4.2 多继承的问题

多继承会带来一些复杂的问题：

**1. 名字冲突**

如果两个基类有同名成员，需要用作用域解析符来区分：

```cpp
class Base1 {
public:
    void show() { cout << "Base1::show" << endl; }
};

class Base2 {
public:
    void show() { cout << "Base2::show" << endl; }
};

class Derived : public Base1, public Base2 {
public:
    void test() {
        Base1::show();  // 明确指定调用哪个
        Base2::show();
    }
};
```

**2. 菱形继承问题**

这是多继承最经典的问题，下一节详细讲。

### 4.3 多继承的内存布局

```
多继承下 Derived 的内存布局：

┌─────────────────┐
│   Base1 部分     │
├─────────────────┤
│   Base2 部分     │
├─────────────────┤
│   Derived 部分   │
└─────────────────┘
```

---

## 五、虚继承——解决菱形问题

### 5.1 什么是菱形继承

```
        Animal
       /      \
    Mammal    Bird
       \      /
       Bat（蝙蝠）
```

蝙蝠同时继承了哺乳动物和鸟类，而两者都继承了动物。如果不做特殊处理，`Bat` 中会有**两份** `Animal` 的数据，这就是**菱形继承问题**。

### 5.2 菱形继承的问题

```cpp
class Animal {
public:
    int age;
};

class Mammal : public Animal {};
class Bird : public Animal {};

class Bat : public Mammal, public Bird {};

Bat b;
b.age = 5;  // 错误！编译器不知道 age 是 Mammal::age 还是 Bird::age
```

### 5.3 虚继承的解决方案

使用 `virtual` 关键字进行虚继承：

```cpp
class Animal {
public:
    int age;
};

class Mammal : virtual public Animal {};  // 虚继承
class Bird : virtual public Animal {};    // 虚继承

class Bat : public Mammal, public Bird {};

Bat b;
b.age = 5;  // OK！现在只有一份 Animal 的数据
```

### 5.4 虚继承的内存布局

```
虚继承后 Bat 的内存布局：

┌─────────────────────┐
│   Mammal 部分        │
│   (vptr → Animal)   │
├─────────────────────┤
│   Bird 部分          │
│   (vptr → Animal)   │
├─────────────────────┤
│   Bat 部分           │
├─────────────────────┤
│   Animal 部分        │  ← 只有一份！
│   (共享)             │
└─────────────────────┘
```

虚继承通过引入**虚基类表（vbtable）**来保证虚基类只有一份实例。Mammal 和 Bird 中各有一个 vptr，指向共享的 Animal 部分。

### 5.5 虚继承的代价

1. **额外的内存开销**：每个虚继承的类需要一个额外的 vptr
2. **性能开销**：访问虚基类成员需要间接寻址
3. **构造更复杂**：虚基类由最远派生类负责构造

> **面试高频：** 什么是菱形继承？如何解决？
>
> 答：菱形继承是指一个派生类同时继承两个基类，而这两个基类又继承自同一个更高层的基类，形成菱形结构。这会导致派生类中存在两份顶层基类的数据。解决方案是使用虚继承（`virtual public`），让顶层基类只保留一份实例。虚继承通过虚基类表实现，会有一定的内存和性能开销。

---

## 六、组合 vs 继承

### 6.1 什么是组合

组合是指一个类包含另一个类的对象作为成员变量，体现的是 **"has-a"** 关系。

```cpp
class Engine {
public:
    void start() { cout << "引擎启动" << endl; }
};

class Car {
private:
    Engine engine;  // Car "has-a" Engine（组合）
    string brand;
public:
    void drive() {
        engine.start();
        cout << brand << " 开始行驶" << endl;
    }
};
```

### 6.2 组合 vs 继承对比

```
┌──────────────────────────────────────────────────────────┐
│                    组合 vs 继承                           │
├──────────────┬───────────────────┬───────────────────────┤
│              │     继承           │     组合               │
├──────────────┼───────────────────┼───────────────────────┤
│ 关系         │ "is-a"            │ "has-a"               │
│ 耦合度       │ 高（白盒复用）      │ 低（黑盒复用）         │
│ 多态支持     │ 天然支持           │ 需要配合接口/指针       │
│ 修改影响     │ 修改基类影响所有子类 │ 修改被组合类影响使用者  │
│ 封装性       │ 破坏封装           │ 保持封装               │
│ 适用场景     │ 真正的"is-a"关系    │ 大多数情况             │
└──────────────┴───────────────────┴───────────────────────┘
```

### 6.3 什么时候用继承，什么时候用组合

**优先使用组合的原则：**

> "Favor composition over inheritance." —— 《设计模式》GoF

**使用继承的场景：**
- 子类确实是父类的一种特殊类型（"is-a"关系明确）
- 需要利用多态特性
- 子类需要访问父类的 protected 成员

**使用组合的场景：**
- 只是想复用某个类的功能（"has-a"关系）
- 不需要多态
- 希望降低耦合度
- 被复用的类可能变化

### 6.4 实际示例

```cpp
// 继承：狗"是一种"动物 —— 合理
class Dog : public Animal {};

// 组合：汽车"有一个"引擎 —— 合理
class Car {
    Engine engine;  // 组合
};

// 错误示范：用继承实现"has-a"
// class Car : public Engine {}  // 汽车"是一种"引擎？不合理！
```

> **面试高频：** 组合和继承怎么选？
>
> 答：面向对象设计的一条重要原则是"组合优于继承"。继承是"is-a"关系，组合是"has-a"关系。继承会带来强耦合，基类的修改可能影响所有子类，而且破坏了封装性。组合则保持了良好的封装和低耦合。只有当确实是"is-a"关系且需要多态时才使用继承，其他情况优先使用组合。

---

## 七、面试真题

### 真题 1：C++ 中 public、protected、private 继承的区别？

**答：** public 继承保持基类成员的访问级别不变，是最常用的继承方式，体现"is-a"关系。protected 继承将基类的 public 和 protected 成员都变为派生类的 protected。private 继承将所有可访问成员变为 private。后两者很少使用，因为它们可以用组合来替代，语义更清晰。

### 真题 2：什么是虚继承？解决什么问题？

**答：** 虚继承通过在继承列表中使用 `virtual` 关键字，解决菱形继承问题。菱形继承是指一个类同时继承两个基类，而这两个基类又继承自同一个更高层的基类，导致派生类中存在两份顶层基类的数据。虚继承保证虚基类只有一份实例，通过虚基类表（vbtable）实现，但会带来额外的内存和性能开销。

### 真题 3：构造函数和析构函数的调用顺序？

**答：** 构造顺序：先调用基类构造函数，再调用成员对象构造函数，最后调用派生类构造函数。析构顺序完全相反：先调用派生类析构函数，再调用成员对象析构函数，最后调用基类析构函数。可以简记为"先有父再有子，析构时先子后父"。

### 真题 4：多继承中如何解决名字冲突？

**答：** 当多个基类有同名成员时，可以通过作用域解析符来明确指定调用哪个基类的成员，例如 `Base1::func()` 和 `Base2::func()`。也可以在派生类中覆盖同名成员来统一接口。

### 真题 5：组合和继承的区别？什么时候用哪个？

**答：** 继承是"is-a"关系，组合是"has-a"关系。继承的耦合度高，基类的修改会影响子类；组合的耦合度低，封装性好。设计原则是"组合优于继承"，只有当确实是"is-a"关系且需要多态时才使用继承，其他情况优先使用组合。

---

## 完整示例代码

本节所有概念的演示代码请参考：

[inheritance_demo.cpp](../../../code/02-cpp-oop/inheritance_demo.cpp)

编译运行：

```bash
g++ -std=c++17 -Wall -o inheritance_demo inheritance_demo.cpp
./inheritance_demo
```

---

## 上一节 | 下一节

[2.1 类与对象](../class/README.md) | [2.3 多态与虚函数](../polymorphism/README.md)

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
