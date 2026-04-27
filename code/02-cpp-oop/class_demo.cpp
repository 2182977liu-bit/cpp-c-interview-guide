/**
 * @file class_demo.cpp
 * @brief 2.1 类与对象 —— 完整示例代码
 * @author 青松与桑叶
 *
 * 编译: g++ -std=c++17 -Wall -o class_demo class_demo.cpp
 * 运行: ./class_demo
 */

#include <iostream>
#include <string>
#include <cstring>
using namespace std;

// ============================================================
// 第一部分：基本类定义
// ============================================================

class Student {
private:
    string name;    // 姓名
    int age;        // 年龄
    double score;   // 成绩

public:
    // 默认构造函数
    Student() : name("未知"), age(0), score(0.0) {
        cout << "默认构造函数被调用" << endl;
    }

    // 带参构造函数（使用初始化列表）
    Student(string n, int a, double s) : name(n), age(a), score(s) {
        cout << "带参构造函数被调用: " << name << endl;
    }

    // 拷贝构造函数
    Student(const Student& other) : name(other.name), age(other.age), score(other.score) {
        cout << "拷贝构造函数被调用" << endl;
    }

    // 移动构造函数（C++11）
    Student(Student&& other) noexcept
        : name(std::move(other.name)), age(other.age), score(other.score) {
        cout << "移动构造函数被调用" << endl;
        other.age = 0;
        other.score = 0.0;
    }

    // 析构函数
    ~Student() {
        cout << "析构函数被调用: " << name << endl;
    }

    // const 成员函数：不修改对象状态
    void display() const {
        cout << "姓名: " << name << ", 年龄: " << age << ", 成绩: " << score << endl;
    }

    // this 指针演示：支持链式调用
    Student& setAge(int a) {
        this->age = a;      // this->age 是成员变量，a 是参数
        return *this;       // 返回对象自身的引用
    }

    Student& setName(const string& n) {
        this->name = n;
        return *this;
    }

    Student& setScore(double s) {
        this->score = s;
        return *this;
    }
};

// ============================================================
// 第二部分：浅拷贝 vs 深拷贝对比
// ============================================================

// 浅拷贝示例（展示问题的原理，不使用自动析构）
class ShallowBuffer {
public:
    char* data;     // 动态内存指针（public 以便演示）
    size_t size;    // 数据大小

    // 构造函数
    ShallowBuffer(const char* str) {
        size = strlen(str) + 1;
        data = new char[size];
        strcpy(data, str);
        cout << "ShallowBuffer 构造: " << data << endl;
    }

    // 注意：这里故意不定义拷贝构造函数
    // 使用编译器默认的浅拷贝

    ~ShallowBuffer() {
        cout << "ShallowBuffer 析构: " << (data ? data : "null") << endl;
        if (data) {
            delete[] data;
            data = nullptr;
        }
    }

    void setData(int index, char c) {
        if (index >= 0 && index < static_cast<int>(size)) {
            data[index] = c;
        }
    }

    void display() const {
        cout << "ShallowBuffer 数据: " << (data ? data : "null") << endl;
    }
};

// 深拷贝示例（正确的版本）
class DeepBuffer {
private:
    char* data;     // 动态内存指针
    size_t size;    // 数据大小

public:
    // 构造函数
    DeepBuffer(const char* str) {
        size = strlen(str) + 1;
        data = new char[size];
        strcpy(data, str);
        cout << "DeepBuffer 构造: " << data << endl;
    }

    // 深拷贝构造函数：分配新内存并复制数据
    DeepBuffer(const DeepBuffer& other) {
        size = other.size;
        data = new char[size];        // 分配独立的内存
        strcpy(data, other.data);     // 复制数据内容
        cout << "DeepBuffer 深拷贝构造: " << data << endl;
    }

    // 深拷贝赋值运算符
    DeepBuffer& operator=(const DeepBuffer& other) {
        cout << "DeepBuffer 深拷贝赋值" << endl;
        if (this != &other) {         // 检查自赋值
            delete[] data;            // 释放原有内存
            size = other.size;
            data = new char[size];    // 分配新内存
            strcpy(data, other.data); // 复制数据
        }
        return *this;
    }

    // 移动构造函数
    DeepBuffer(DeepBuffer&& other) noexcept
        : data(other.data), size(other.size) {
        cout << "DeepBuffer 移动构造" << endl;
        other.data = nullptr;   // 将源对象置为安全状态
        other.size = 0;
    }

    // 析构函数
    ~DeepBuffer() {
        cout << "DeepBuffer 析构: " << (data ? data : "null") << endl;
        delete[] data;
    }

    void setData(int index, char c) {
        if (index >= 0 && index < static_cast<int>(size)) {
            data[index] = c;
        }
    }

    void display() const {
        cout << "DeepBuffer 数据: " << (data ? data : "null") << endl;
    }
};

// ============================================================
// main 函数
// ============================================================

int main() {
    cout << "========== 第一部分：基本类定义与使用 ==========" << endl;

    // 默认构造
    Student s1;
    // 带参构造
    Student s2("张三", 20, 95.5);
    // 拷贝构造
    Student s3 = s2;
    // 移动构造
    Student s4 = std::move(s1);

    // 显示信息
    s2.display();
    s3.display();

    // this 指针：链式调用
    cout << "\n--- 链式调用演示 ---" << endl;
    Student s5;
    s5.setAge(22).setName("李四").setScore(88.0).display();

    cout << "\n========== 第二部分：浅拷贝 vs 深拷贝 ==========" << endl;

    // --- 浅拷贝问题演示 ---
    cout << "\n--- 浅拷贝（有 bug 的版本）---" << endl;
    {
        ShallowBuffer buf1("Hello");
        // 模拟浅拷贝：手动复制指针值（两个指针指向同一块内存）
        ShallowBuffer buf2("placeholder");
        delete[] buf2.data;  // 释放 buf2 原有的内存
        buf2.data = buf1.data;  // 浅拷贝：直接复制指针
        buf2.size = buf1.size;

        buf1.display();
        buf2.display();
        cout << "  buf1.data 地址: " << static_cast<void*>(buf1.data) << endl;
        cout << "  buf2.data 地址: " << static_cast<void*>(buf2.data) << endl;
        cout << "  （两个指针指向同一块内存！）" << endl;

        // 修改 buf2 会影响 buf1（因为共享内存）
        cout << "修改 buf2 的第一个字符为 'X'" << endl;
        buf2.setData(0, 'X');
        buf1.display();   // buf1 的数据也被修改了！
        buf2.display();

        // 手动释放：只能 delete 一次！
        buf1.data = nullptr;  // 先置空 buf1 的指针
        buf2.data = nullptr;  // 再置空 buf2 的指针
        // 实际内存由 buf2 的析构函数释放（但我们已置空，所以不会 double free）
        // 注意：这里原始内存泄漏了！这正是浅拷贝的问题之一
    }
    cout << "（浅拷贝导致：数据污染 + 内存管理困难）" << endl;

    // --- 深拷贝正确演示 ---
    cout << "\n--- 深拷贝（正确的版本）---" << endl;
    {
        DeepBuffer buf1("Hello");
        // 使用自定义的深拷贝构造函数
        // buf1 和 buf2 各自拥有独立的内存
        DeepBuffer buf2 = buf1;

        buf1.display();
        buf2.display();

        // 修改 buf2 不会影响 buf1（因为内存独立）
        cout << "修改 buf2 的第一个字符为 'X'" << endl;
        buf2.setData(0, 'X');
        buf1.display();   // buf1 的数据不受影响
        buf2.display();

        // 深拷贝赋值
        cout << "\n--- 深拷贝赋值演示 ---" << endl;
        DeepBuffer buf3("World");
        buf3 = buf1;      // 调用深拷贝赋值运算符
        buf3.display();
    }
    // buf1 和 buf2 各自析构，各自释放自己的内存，安全！

    cout << "\n========== 程序结束 ==========" << endl;
    return 0;
}
