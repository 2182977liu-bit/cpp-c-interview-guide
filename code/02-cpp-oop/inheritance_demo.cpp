/**
 * @file inheritance_demo.cpp
 * @brief 2.2 继承与组合 —— 完整示例代码
 * @author 青松与桑叶
 *
 * 编译: g++ -std=c++17 -Wall -o inheritance_demo inheritance_demo.cpp
 * 运行: ./inheritance_demo
 */

#include <iostream>
#include <string>
using namespace std;

// ============================================================
// 第一部分：单继承
// ============================================================

// 基类：动物
class Animal {
protected:
    string name;    // protected：子类可以访问
    int age;

public:
    Animal(const string& n, int a) : name(n), age(a) {
        cout << "Animal 构造: " << name << endl;
    }

    virtual ~Animal() {    // 虚析构函数（重要！）
        cout << "Animal 析构: " << name << endl;
    }

    void eat() const {
        cout << name << " 在吃东西" << endl;
    }

    virtual void speak() const {   // 虚函数，支持多态
        cout << name << " 发出声音" << endl;
    }
};

// 派生类：狗（单继承）
class Dog : public Animal {
private:
    string breed;   // 品种

public:
    Dog(const string& n, int a, const string& b)
        : Animal(n, a), breed(b) {
        cout << "Dog 构造: " << name << " (" << breed << ")" << endl;
    }

    ~Dog() override {
        cout << "Dog 析构: " << name << endl;
    }

    void speak() const override {   // 重写虚函数
        cout << name << " 汪汪汪！" << endl;
    }

    void fetch() const {
        cout << name << " 在捡球" << endl;
    }
};

// 派生类：猫（单继承）
class Cat : public Animal {
private:
    bool isIndoor;  // 是否是室内猫

public:
    Cat(const string& n, int a, bool indoor)
        : Animal(n, a), isIndoor(indoor) {
        cout << "Cat 构造: " << name << endl;
    }

    ~Cat() override {
        cout << "Cat 析构: " << name << endl;
    }

    void speak() const override {
        cout << name << " 喵喵喵！" << endl;
    }
};

// ============================================================
// 第二部分：多继承
// ============================================================

// 基类1：教师
class Teacher {
public:
    Teacher() { cout << "Teacher 构造" << endl; }
    virtual ~Teacher() { cout << "Teacher 析构" << endl; }

    void teach() const {
        cout << "老师在讲课" << endl;
    }

    virtual void work() const {
        cout << "Teacher::work() - 教学" << endl;
    }
};

// 基类2：研究员
class Researcher {
public:
    Researcher() { cout << "Researcher 构造" << endl; }
    virtual ~Researcher() { cout << "Researcher 析构" << endl; }

    void research() const {
        cout << "在做研究" << endl;
    }

    virtual void work() const {
        cout << "Researcher::work() - 研究" << endl;
    }
};

// 多继承：教授同时是教师和研究员
class Professor : public Teacher, public Researcher {
private:
    string title;

public:
    Professor(const string& t) : title(t) {
        cout << "Professor 构造: " << title << endl;
    }

    ~Professor() override {
        cout << "Professor 析构: " << title << endl;
    }

    void supervise() const {
        cout << title << " 在指导学生" << endl;
    }

    // 解决多继承中的名字冲突：必须明确指定调用哪个基类的方法
    void showWork() const {
        cout << "作为教师: ";
        Teacher::work();    // 明确调用 Teacher 的 work
        cout << "作为研究员: ";
        Researcher::work(); // 明确调用 Researcher 的 work
    }
};

// ============================================================
// 第三部分：虚继承 —— 解决菱形问题
// ============================================================

// 顶层基类
class AnimalBase {
public:
    AnimalBase() { cout << "AnimalBase 构造" << endl; }
    virtual ~AnimalBase() { cout << "AnimalBase 析构" << endl; }

    void setAge(int a) { age = a; }
    int getAge() const { return age; }

protected:
    int age = 0;
};

// 中间层：使用虚继承
class Mammal : virtual public AnimalBase {
public:
    Mammal() { cout << "Mammal 构造" << endl; }
    virtual ~Mammal() { cout << "Mammal 析构" << endl; }
};

class Bird : virtual public AnimalBase {
public:
    Bird() { cout << "Bird 构造" << endl; }
    virtual ~Bird() { cout << "Bird 析构" << endl; }
};

// 最远派生类：蝙蝠（菱形继承）
class Bat : public Mammal, public Bird {
public:
    Bat() {
        cout << "Bat 构造" << endl;
        // 虚继承后，AnimalBase 只有一份，age 不会产生歧义
        age = 3;
        cout << "蝙蝠年龄: " << age << endl;
    }

    ~Bat() override {
        cout << "Bat 析构" << endl;
    }

    void fly() const {
        cout << "蝙蝠在飞行" << endl;
    }
};

// ============================================================
// 第四部分：组合 vs 继承
// ============================================================

// 组合示例：引擎类
class Engine {
private:
    string type;
    int horsepower;

public:
    Engine(const string& t, int hp) : type(t), horsepower(hp) {
        cout << "Engine 构造: " << type << " " << horsepower << "马力" << endl;
    }

    ~Engine() {
        cout << "Engine 析构: " << type << endl;
    }

    void start() const {
        cout << type << " 引擎启动，" << horsepower << "马力运转中" << endl;
    }

    void stop() const {
        cout << type << " 引擎关闭" << endl;
    }
};

// 组合示例：汽车类（has-a 关系）
class Car {
private:
    string brand;
    Engine engine;   // 组合：Car "拥有" 一个 Engine

public:
    Car(const string& b, const string& engineType, int hp)
        : brand(b), engine(engineType, hp) {
        cout << "Car 构造: " << brand << endl;
    }

    ~Car() {
        cout << "Car 析构: " << brand << endl;
    }

    void drive() const {
        cout << brand << " 准备出发..." << endl;
        engine.start();    // 通过组合的对象调用其方法
        cout << brand << " 在行驶中" << endl;
    }

    void park() const {
        cout << brand << " 准备停车..." << endl;
        engine.stop();
        cout << brand << " 已停好" << endl;
    }
};

// ============================================================
// main 函数
// ============================================================

int main() {
    cout << "========== 第一部分：单继承 ==========" << endl;
    {
        Dog dog("旺财", 3, "金毛");
        dog.eat();     // 继承自 Animal
        dog.speak();   // 重写的虚函数
        dog.fetch();   // Dog 自己的方法

        Cat cat("咪咪", 2, true);
        cat.eat();
        cat.speak();

        // 多态演示：基类指针指向派生类对象
        cout << "\n--- 多态演示 ---" << endl;
        Animal* animals[] = {new Dog("大黄", 4, "中华田园犬"), new Cat("花花", 1, false)};
        for (auto a : animals) {
            a->speak();    // 动态绑定，调用实际类型的 speak()
            delete a;      // 虚析构函数确保正确析构
        }
    }

    cout << "\n========== 第二部分：多继承 ==========" << endl;
    {
        Professor prof("王教授");
        prof.teach();       // 继承自 Teacher
        prof.research();    // 继承自 Researcher
        prof.supervise();   // Professor 自己的方法
        prof.showWork();    // 解决名字冲突
    }

    cout << "\n========== 第三部分：虚继承（菱形问题）==========" << endl;
    {
        Bat bat;
        bat.setAge(5);
        cout << "蝙蝠年龄: " << bat.getAge() << endl;
        bat.fly();
        // 虚继承后，AnimalBase 只有一份，不会产生二义性
    }

    cout << "\n========== 第四部分：组合 vs 继承 ==========" << endl;
    {
        cout << "--- 组合示例 ---" << endl;
        Car car("特斯拉", "电动", 450);
        car.drive();
        cout << endl;
        car.park();
        // Car 析构时，Engine 也会自动析构（组合的生命周期管理）
    }

    cout << "\n========== 程序结束 ==========" << endl;
    return 0;
}
