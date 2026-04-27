# 2.1 类与对象

> 作者：青松与桑叶
> C/C++ 面试通关指南 -- 第二部分：C++ 面向对象

---

## 目录

- [一、什么是类和对象](#一什么是类和对象)
- [二、类的定义](#二类的定义)
- [三、构造函数](#三构造函数)
- [四、析构函数](#四析构函数)
- [五、this 指针](#五this-指针)
- [六、拷贝控制：深拷贝 vs 浅拷贝](#六拷贝控制深拷贝-vs-浅拷贝)
- [七、面试真题](#七面试真题)

---

## 一、什么是类和对象

### 1.1 从生活理解类和对象

同学们好！在正式讲代码之前，我们先从生活中的例子来理解"类"和"对象"。

想象一下，你手里有一张**建筑图纸**，上面画着一栋三室两厅的房子，标注了每个房间的用途。这张图纸就是**类（Class）**——它只是一个**设计蓝图**，描述了"房子应该长什么样"。

而根据这张图纸，施工队实际盖出来的那一栋栋具体的房子，就是**对象（Object）**——它们是图纸的**具体实例**。

再举几个例子：

| 类（蓝图） | 对象（实例） |
|-----------|-------------|
| `Student`（学生类） | 张三、李四、王五 |
| `Car`（汽车类） | 你家的那辆白色丰田、隔壁的黑色本田 |
| `Phone`（手机类） | 你的 iPhone 15、同事的华为 Mate 60 |

### 1.2 类和对象的关系

```
类（Class）  ──实例化──>  对象（Object）
  模板/蓝图                   具体的实体
  抽象的概念                   占用实际内存
  不占数据内存                 占用数据内存
```

**一句话总结：类是对象的类型，对象是类的实例。**

### 1.3 面试怎么答

> **面试官：请解释一下类和对象的区别。**
>
> 答：类是一种用户自定义的数据类型，它定义了数据成员（属性）和成员函数（方法）。对象是类的具体实例，在内存中占有实际空间。类本身不占用数据内存（只有代码段），而每个对象都有自己的数据副本。打个比方，类就像建筑图纸，对象就是根据图纸建造的房子。

---

## 二、类的定义

### 2.1 基本语法

C++ 中用 `class` 关键字来定义类：

```cpp
class 类名 {
public:       // 公有成员，外部可以访问
    // 成员函数声明
protected:    // 保护成员，子类可以访问
    // ...
private:      // 私有成员，只有类内部可以访问
    // 成员变量声明
};
```

### 2.2 成员变量和成员函数

```cpp
class Student {
private:
    string name;    // 成员变量（属性）
    int age;
    double score;

public:
    // 成员函数（方法）
    void display() const {
        cout << "姓名: " << name << ", 年龄: " << age << endl;
    }

    // setter 方法
    void setName(const string& n) {
        name = n;
    }

    // getter 方法
    string getName() const {
        return name;
    }
};
```

### 2.3 访问控制修饰符

C++ 提供三种访问控制：

| 修饰符 | 类内部 | 子类 | 外部代码 |
|--------|--------|------|----------|
| `public` | 可以 | 可以 | 可以 |
| `protected` | 可以 | 可以 | 不可以 |
| `private` | 可以 | 不可以 | 不可以 |

**设计原则：**
- 成员变量通常设为 `private`（封装性）
- 需要给外部使用的函数设为 `public`
- 需要给子类使用的设为 `protected`

> **为什么要把成员变量设为 private？**
> 这是面向对象三大特性之一——**封装**。好处是：
> 1. 可以控制访问权限（比如只读）
> 2. 可以在修改时加入校验逻辑
> 3. 内部实现改变不影响外部代码

### 2.4 类的声明和实现分离

对于大型项目，通常把类的声明放在头文件（`.h`），实现放在源文件（`.cpp`）：

```cpp
// Student.h —— 声明
class Student {
private:
    string name;
    int age;
public:
    void display() const;
    void setAge(int a);
};

// Student.cpp —— 实现
void Student::display() const {
    cout << "姓名: " << name << ", 年龄: " << age << endl;
}

void Student::setAge(int a) {
    age = a;
}
```

注意 `Student::display()` 中的 `::` 是**作用域解析符**，表示 `display` 是 `Student` 类的成员函数。

---

## 三、构造函数

构造函数是一种特殊的成员函数，在**创建对象时自动调用**。它的名字和类名相同，没有返回值。

### 3.1 默认构造函数

```cpp
class Student {
public:
    // 默认构造函数：不带参数
    Student() : name("未知"), age(0), score(0.0) {
        cout << "默认构造函数被调用" << endl;
    }
};
```

**注意：** 如果你没有定义任何构造函数，编译器会自动生成一个默认构造函数（什么都不做）。但如果你定义了带参构造函数，编译器就**不再自动生成**默认构造函数了。

### 3.2 带参构造函数

```cpp
Student(string n, int a, double s) : name(n), age(a), score(s) {
    cout << "带参构造函数被调用: " << name << endl;
}
```

**初始化列表** `: name(n), age(a)` 比在函数体内赋值更高效，尤其是对于 `const` 成员和引用成员，**必须**使用初始化列表。

### 3.3 拷贝构造函数

当一个对象需要用另一个同类型对象来初始化时，拷贝构造函数被调用：

```cpp
Student(const Student& other) : name(other.name), age(other.age), score(other.score) {
    cout << "拷贝构造函数被调用" << endl;
}
```

**拷贝构造函数在以下场景被调用：**
1. 用一个对象初始化另一个对象：`Student s2 = s1;`
2. 函数参数按值传递对象
3. 函数按值返回对象

### 3.4 移动构造函数（C++11）

移动构造函数"偷取"源对象的资源，而不是拷贝：

```cpp
Student(Student&& other) noexcept
    : name(std::move(other.name)),
      age(other.age),
      score(other.score) {
    cout << "移动构造函数被调用" << endl;
    other.age = 0;       // 将源对象置于有效但不确定的状态
    other.score = 0.0;
}
```

**什么时候用移动构造？** 当源对象是临时的（右值），你不需要保留它时。移动比拷贝高效得多，因为不需要分配新内存和复制数据。

### 3.5 委托构造函数（C++11）

一个构造函数可以调用同类的另一个构造函数：

```cpp
class Student {
public:
    Student() : Student("未知", 0, 0.0) {}  // 委托给带参构造
    Student(string n, int a, double s) : name(n), age(a), score(s) {}
};
```

### 3.6 构造函数小结

```
构造函数类型          语法特征                    触发时机
─────────────────────────────────────────────────────────
默认构造函数          无参数或所有参数有默认值      Student s;
带参构造函数          有参数                      Student s("张三", 20, 95.5);
拷贝构造函数          参数是 const T&             Student s2 = s1;
移动构造函数          参数是 T&&（C++11）          Student s2 = std::move(s1);
委托构造函数          初始化列表调用另一个构造     减少重复代码
```

---

## 四、析构函数

析构函数在对象**生命周期结束时自动调用**，用于释放资源。

```cpp
~Student() {
    cout << "析构函数被调用: " << name << endl;
}
```

### 4.1 析构函数的特点

1. 名字是 `~类名`
2. 没有参数，没有返回值
3. 不能重载（一个类只有一个析构函数）
4. 在以下时机被调用：
   - 局部对象离开作用域时
   - `delete` 动态分配的对象时
   - 程序结束时（全局/静态对象）

### 4.2 析构顺序

析构的顺序和构造的顺序**相反**：

```cpp
void test() {
    Student s1;           // 先构造 s1
    Student s2("李四", 21, 88.0);  // 再构造 s2
}  // 先析构 s2，再析构 s1（栈的后进先出）
```

### 4.3 虚析构函数

如果类可能被继承，并且会通过基类指针删除派生类对象，**必须将析构函数声明为虚函数**：

```cpp
class Base {
public:
    virtual ~Base() { cout << "Base 析构" << endl; }
};

class Derived : public Base {
public:
    ~Derived() override { cout << "Derived 析构" << endl; }
};

Base* p = new Derived();
delete p;  // 如果 Base 的析构不是虚函数，Derived 的析构不会被调用！
```

> **面试高频考点：** 为什么基类的析构函数要声明为虚函数？如果不声明为虚函数，通过基类指针 `delete` 派生类对象时，只会调用基类的析构函数，派生类的资源不会被释放，造成**内存泄漏**。

---

## 五、this 指针

### 5.1 什么是 this 指针

每个成员函数都有一个隐藏的参数 `this`，它是一个指向**调用该函数的对象**的指针。

```cpp
class Student {
private:
    string name;
    int age;
public:
    void setName(string name) {
        // 这里的 name 是参数，this->name 是成员变量
        this->name = name;
    }
    void setAge(int age) {
        this->age = age;
    }
};
```

### 5.2 this 指针的本质

```cpp
Student s;
s.setName("张三");
// 编译器实际处理为：
setName(&s, "张三");  // this = &s
```

### 5.3 this 指针的常见用途

**1. 区分成员变量和参数同名**

```cpp
void setAge(int age) {
    this->age = age;  // this->age 是成员变量，age 是参数
}
```

**2. 返回对象自身，支持链式调用**

```cpp
Student& setAge(int a) {
    this->age = a;
    return *this;  // 返回对象自身的引用
}

// 使用：
s1.setAge(20).setName("张三").display();
```

**3. 在成员函数中返回对象的指针**

```cpp
Student* getThis() {
    return this;
}
```

### 5.4 this 指针注意事项

- `this` 指针是 `const` 的（不能修改 `this` 的指向）
- 在 `const` 成员函数中，`this` 的类型是 `const ClassName* const`
- 静态成员函数**没有** `this` 指针（因为它不属于任何具体对象）

---

## 六、拷贝控制：深拷贝 vs 浅拷贝

### 6.1 什么是浅拷贝

如果你没有自定义拷贝构造函数，编译器会生成一个**默认的拷贝构造函数**，它执行的是**浅拷贝**——逐字节复制成员变量。

对于只包含简单类型（int、double、string 等）的类，浅拷贝完全够用。但如果类中包含**指针**，浅拷贝会导致两个对象指向同一块内存，这就是灾难的开始。

### 6.2 浅拷贝的问题

```
浅拷贝前：
  obj1.ptr ──> [动态内存: "Hello"]

浅拷贝后：
  obj1.ptr ──> [动态内存: "Hello"] <── obj2.ptr
                    ↑
              两个指针指向同一块内存！

问题：
  1. 修改 obj2 会影响 obj1（数据污染）
  2. obj1 析构时释放了内存
  3. obj2 析构时再次释放同一块内存 → double free！
```

### 6.3 深拷贝的解决方案

深拷贝会为每个对象分配独立的内存，并复制数据：

```cpp
class Buffer {
private:
    char* data;
    size_t size;
public:
    // 构造函数
    Buffer(const char* str) {
        size = strlen(str) + 1;
        data = new char[size];
        strcpy(data, str);
    }

    // 深拷贝构造函数
    Buffer(const Buffer& other) {
        size = other.size;
        data = new char[size];        // 分配新内存
        strcpy(data, other.data);     // 复制数据
    }

    // 析构函数
    ~Buffer() {
        delete[] data;
    }
};
```

### 6.4 Rule of Three / Rule of Five

如果你需要自定义以下任何一个，通常需要自定义全部：

**C++98 的 Rule of Three（三法则）：**
1. 析构函数
2. 拷贝构造函数
3. 拷贝赋值运算符

**C++11 的 Rule of Five（五法则）：**
1. 析构函数
2. 拷贝构造函数
3. 拷贝赋值运算符
4. 移动构造函数
5. 移动赋值运算符

> **面试高频：** 解释深拷贝和浅拷贝的区别。
>
> 答：浅拷贝只是简单地复制内存中的值，如果类中包含指针，浅拷贝会导致多个对象共享同一块动态内存，引发 double free 和数据污染。深拷贝会为指针成员分配新的内存并复制数据，确保每个对象拥有独立的资源。当类管理动态内存等资源时，必须实现深拷贝。

---

## 七、面试真题

### 真题 1：构造函数可以是虚函数吗？

**答：** 不可以。虚函数的机制依赖于虚函数表（vtable），而 vtable 指针（vptr）是在构造函数中初始化的。如果构造函数是虚函数，那么在调用构造函数时 vptr 还没有初始化，无法找到虚函数表，产生矛盾。此外，构造函数的作用是初始化对象，在构造完成之前对象还没有确定的具体类型。

### 真题 2：拷贝构造函数和赋值运算符有什么区别？

**答：**
- **拷贝构造函数**是用一个已存在的对象来**创建**一个新对象：`Student s2(s1);`
- **赋值运算符**是将一个对象的值**赋给**一个已存在的对象：`s2 = s1;`
- 拷贝构造函数不需要检查自赋值，因为新对象还没有被初始化
- 赋值运算符需要检查自赋值（`if (this == &other) return *this;`），并且需要释放原有资源

### 真题 3：什么情况下编译器会自动生成拷贝构造函数？什么情况下不会？

**答：** 如果你没有声明任何拷贝控制成员（拷贝构造、拷贝赋值、移动构造、移动赋值、析构），编译器会生成默认的拷贝构造函数。但如果你声明了以下任何一项：
- 拷贝构造函数
- 拷贝赋值运算符
- 移动构造函数
- 移动赋值运算符
- 析构函数

编译器就不再自动生成拷贝构造函数（具体规则取决于 C++ 标准版本，C++11 之后规则有所变化）。

### 真题 4：explicit 关键字的作用是什么？

**答：** `explicit` 关键字用于阻止构造函数的**隐式转换**。例如：

```cpp
class Student {
public:
    explicit Student(string n) : name(n) {}
};

Student s1("张三");     // OK，直接初始化
Student s2 = "李四";    // 错误！被 explicit 阻止
```

不加 `explicit` 时，`Student s2 = "李四"` 会隐式调用构造函数，这可能导致难以察觉的 bug。**单参数构造函数建议加 `explicit`。**

### 真题 5：static 成员变量和 static 成员函数的特点？

**答：**
- `static` 成员变量属于类级别，所有对象共享同一份数据，必须在类外初始化
- `static` 成员函数没有 `this` 指针，只能访问 `static` 成员
- 可以通过 `ClassName::staticFunc()` 直接调用，不需要创建对象
- `static` 成员变量在程序启动时分配内存，程序结束时释放

---

## 完整示例代码

本节所有概念的演示代码请参考：

[class_demo.cpp](../../../code/02-cpp-oop/class_demo.cpp)

编译运行：

```bash
g++ -std=c++17 -Wall -o class_demo class_demo.cpp
./class_demo
```

---

## 下一节

[2.2 继承与组合](../inheritance/README.md)

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
