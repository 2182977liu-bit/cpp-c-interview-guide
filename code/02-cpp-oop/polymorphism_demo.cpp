/**
 * @file polymorphism_demo.cpp
 * @brief 2.3 多态与虚函数 —— 完整示例代码
 * @author 青松与桑叶
 *
 * 编译: g++ -std=c++17 -Wall -o polymorphism_demo polymorphism_demo.cpp
 * 运行: ./polymorphism_demo
 */

#include <iostream>
#include <string>
#include <vector>
#include <cmath>
using namespace std;

// ============================================================
// 第一部分：虚函数基本使用
// ============================================================

class Animal {
protected:
    string name;

public:
    Animal(const string& n) : name(n) {}

    // 虚函数：支持动态绑定
    virtual void speak() const {
        cout << name << " 发出声音" << endl;
    }

    // 虚析构函数：确保通过基类指针删除时正确析构
    virtual ~Animal() {
        cout << "Animal 析构: " << name << endl;
    }
};

class Dog : public Animal {
public:
    Dog(const string& n) : Animal(n) {}

    // override 关键字：明确表示重写基类虚函数（C++11）
    void speak() const override {
        cout << name << " 汪汪汪！" << endl;
    }

    ~Dog() override {
        cout << "Dog 析构: " << name << endl;
    }
};

class Cat : public Animal {
public:
    Cat(const string& n) : Animal(n) {}

    void speak() const override {
        cout << name << " 喵喵喵！" << endl;
    }

    ~Cat() override {
        cout << "Cat 析构: " << name << endl;
    }
};

class Duck : public Animal {
public:
    Duck(const string& n) : Animal(n) {}

    void speak() const override {
        cout << name << " 嘎嘎嘎！" << endl;
    }

    ~Duck() override {
        cout << "Duck 析构: " << name << endl;
    }
};

// ============================================================
// 第二部分：纯虚函数和抽象类
// ============================================================

// 抽象类：图形（含有纯虚函数，不能实例化）
class Shape {
public:
    virtual ~Shape() = default;

    // 纯虚函数：子类必须实现
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual void draw() const = 0;

    // 普通虚函数：子类可以选择重写
    virtual string description() const {
        return "这是一个图形";
    }
};

// 具体类：圆形
class Circle : public Shape {
private:
    double radius;

public:
    explicit Circle(double r) : radius(r) {}

    double area() const override {
        return M_PI * radius * radius;
    }

    double perimeter() const override {
        return 2 * M_PI * radius;
    }

    void draw() const override {
        cout << "画一个半径为 " << radius << " 的圆" << endl;
    }

    string description() const override {
        return "圆形 (r=" + to_string(radius) + ")";
    }
};

// 具体类：矩形
class Rectangle : public Shape {
private:
    double width;
    double height;

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

    string description() const override {
        return "矩形 (" + to_string(width) + "x" + to_string(height) + ")";
    }
};

// 具体类：三角形
class Triangle : public Shape {
private:
    double a, b, c;  // 三条边

public:
    Triangle(double sideA, double sideB, double sideC)
        : a(sideA), b(sideB), c(sideC) {}

    double area() const override {
        // 海伦公式
        double s = (a + b + c) / 2.0;
        return sqrt(s * (s - a) * (s - b) * (s - c));
    }

    double perimeter() const override {
        return a + b + c;
    }

    void draw() const override {
        cout << "画一个边长为 " << a << ", " << b << ", " << c << " 的三角形" << endl;
    }

    string description() const override {
        return "三角形 (" + to_string(a) + "," + to_string(b) + "," + to_string(c) + ")";
    }
};

// ============================================================
// 第三部分：动态绑定 vs 静态绑定
// ============================================================

class Base {
public:
    // 虚函数 —— 动态绑定
    virtual void virtualFunc() const {
        cout << "Base::virtualFunc() [虚函数 - 动态绑定]" << endl;
    }

    // 普通函数 —— 静态绑定
    void normalFunc() const {
        cout << "Base::normalFunc() [普通函数 - 静态绑定]" << endl;
    }

    virtual ~Base() = default;
};

class DerivedStatic : public Base {
public:
    void virtualFunc() const override {
        cout << "DerivedStatic::virtualFunc() [虚函数 - 动态绑定]" << endl;
    }

    void normalFunc() const {
        cout << "DerivedStatic::normalFunc() [普通函数 - 静态绑定]" << endl;
    }
};

// ============================================================
// 第四部分：override 和 final
// ============================================================

class BaseClass {
public:
    virtual void func() const {
        cout << "BaseClass::func()" << endl;
    }

    virtual void process() const {
        cout << "BaseClass::process()" << endl;
    }

    // final 函数：禁止子类重写
    virtual void finalFunc() const final {
        cout << "BaseClass::finalFunc() —— 不能被重写" << endl;
    }

    virtual ~BaseClass() = default;
};

class MiddleClass : public BaseClass {
public:
    void func() const override {
        cout << "MiddleClass::func() [override]" << endl;
    }

    void process() const override {
        cout << "MiddleClass::process() [override]" << endl;
    }

    // 以下写法会导致编译错误（final 函数不能被重写）：
    // void finalFunc() const override { }

    virtual ~MiddleClass() = default;
};

// final 类：禁止被继承
class FinalClass final {
public:
    void doSomething() const {
        cout << "FinalClass::doSomething()" << endl;
    }
};

// 以下写法会导致编译错误（final 类不能被继承）：
// class DerivedFromFinal : public FinalClass { };

// ============================================================
// main 函数
// ============================================================

int main() {
    cout << "========== 第一部分：虚函数基本使用 ==========" << endl;

    // 直接调用（静态绑定，虽然 speak 是虚函数）
    Dog dog("旺财");
    dog.speak();

    // 通过基类指针调用（动态绑定）
    cout << "\n--- 通过基类指针调用（动态绑定）---" << endl;
    Animal* animals[] = {
        new Dog("大黄"),
        new Cat("咪咪"),
        new Duck("唐老鸭")
    };

    for (auto a : animals) {
        a->speak();    // 运行时根据实际类型调用
    }

    // 清理
    for (auto a : animals) {
        delete a;      // 虚析构函数确保正确析构
    }

    // 通过基类引用调用（也是动态绑定）
    cout << "\n--- 通过基类引用调用（动态绑定）---" << endl;
    Dog dog2("小黑");
    Animal& ref = dog2;
    ref.speak();       // 调用 Dog::speak()

    cout << "\n========== 第二部分：纯虚函数和抽象类 ==========" << endl;

    // Shape s;  // 错误！抽象类不能实例化

    // 使用多态处理不同的图形
    vector<Shape*> shapes = {
        new Circle(5.0),
        new Rectangle(4.0, 6.0),
        new Triangle(3.0, 4.0, 5.0)
    };

    double totalArea = 0.0;
    for (const auto& shape : shapes) {
        shape->draw();
        cout << "  " << shape->description() << endl;
        cout << "  面积: " << shape->area() << endl;
        cout << "  周长: " << shape->perimeter() << endl;
        totalArea += shape->area();
        cout << endl;
    }
    cout << "所有图形的总面积: " << totalArea << endl;

    // 清理
    for (auto shape : shapes) {
        delete shape;
    }

    cout << "\n========== 第三部分：动态绑定 vs 静态绑定 ==========" << endl;

    DerivedStatic derived;
    Base* ptr = &derived;

    cout << "通过基类指针调用:" << endl;
    ptr->virtualFunc();   // 动态绑定 → DerivedStatic::virtualFunc()
    ptr->normalFunc();    // 静态绑定 → Base::normalFunc()（看指针类型）

    cout << "\n通过派生类对象直接调用:" << endl;
    derived.virtualFunc();  // 直接调用 → DerivedStatic::virtualFunc()
    derived.normalFunc();   // 直接调用 → DerivedStatic::normalFunc()

    cout << "\n========== 第四部分：override 和 final ==========" << endl;

    MiddleClass middle;
    BaseClass* basePtr = &middle;

    basePtr->func();       // 动态绑定 → MiddleClass::func()
    basePtr->process();    // 动态绑定 → MiddleClass::process()
    basePtr->finalFunc();  // 调用 BaseClass::finalFunc()（不能被重写）

    cout << "\nFinalClass 不能被继承，finalFunc 不能被重写" << endl;
    FinalClass finalObj;
    finalObj.doSomething();

    cout << "\n========== 程序结束 ==========" << endl;
    return 0;
}
