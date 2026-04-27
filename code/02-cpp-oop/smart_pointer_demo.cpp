/**
 * @file smart_pointer_demo.cpp
 * @brief 2.4 智能指针 —— 完整示例代码
 * @author 青松与桑叶
 *
 * 编译: g++ -std=c++17 -Wall -o smart_pointer_demo smart_pointer_demo.cpp
 * 运行: ./smart_pointer_demo
 */

#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace std;

// ============================================================
// 辅助类：用于观察构造和析构
// ============================================================

class Widget {
private:
    string name;

public:
    explicit Widget(const string& n) : name(n) {
        cout << "  Widget 构造: " << name << endl;
    }

    ~Widget() {
        cout << "  Widget 析构: " << name << endl;
    }

    void show() const {
        cout << "  Widget::show() - " << name << endl;
    }
};

// ============================================================
// 第一部分：unique_ptr —— 独占所有权
// ============================================================

void demo_unique_ptr() {
    cout << "===== unique_ptr 演示 =====" << endl;

    // 1. 创建 unique_ptr（推荐使用 make_unique）
    cout << "\n--- 创建 unique_ptr ---" << endl;
    auto p1 = make_unique<Widget>("Widget-A");
    p1->show();

    // 2. unique_ptr 不可拷贝
    cout << "\n--- unique_ptr 不可拷贝 ---" << endl;
    // auto p2 = p1;  // 编译错误！unique_ptr 不可拷贝
    cout << "  （auto p2 = p1; 会导致编译错误）" << endl;

    // 3. unique_ptr 可以移动
    cout << "\n--- unique_ptr 移动语义 ---" << endl;
    auto p2 = std::move(p1);   // p1 的所有权转移给 p2
    cout << "  p1 是否为空: " << (p1 == nullptr ? "是" : "否") << endl;
    cout << "  p2 是否为空: " << (p2 == nullptr ? "是" : "否") << endl;
    p2->show();

    // 4. 获取原始指针
    cout << "\n--- 获取原始指针 ---" << endl;
    Widget* raw = p2.get();
    cout << "  原始指针地址: " << raw << endl;

    // 5. release()：释放所有权（不删除对象）
    cout << "\n--- release() 释放所有权 ---" << endl;
    Widget* released = p2.release();  // p2 变为 nullptr，不删除对象
    cout << "  p2 是否为空: " << (p2 == nullptr ? "是" : "否") << endl;
    cout << "  通过 released 指针调用:" << endl;
    released->show();
    delete released;  // 需要手动释放！

    // 6. reset()：重置指针
    cout << "\n--- reset() 重置指针 ---" << endl;
    auto p3 = make_unique<Widget>("Widget-B");
    cout << "  重置前..." << endl;
    p3.reset();  // 删除 Widget-B，p3 变为 nullptr
    cout << "  重置后 p3 是否为空: " << (p3 == nullptr ? "是" : "否") << endl;

    // 7. unique_ptr 管理数组
    cout << "\n--- unique_ptr 管理数组 ---" << endl;
    auto arr = make_unique<int[]>(5);
    for (int i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }
    cout << "  数组内容: ";
    for (int i = 0; i < 5; i++) {
        cout << arr[i] << " ";
    }
    cout << endl;

    cout << "\n--- unique_ptr 离开作用域自动释放 ---" << endl;
    auto p4 = make_unique<Widget>("Widget-C");
    // p4 离开作用域时自动析构
}

// ============================================================
// 第二部分：shared_ptr —— 共享所有权
// ============================================================

void demo_shared_ptr() {
    cout << "\n===== shared_ptr 演示 =====" << endl;

    // 1. 创建 shared_ptr
    cout << "\n--- 创建 shared_ptr ---" << endl;
    auto p1 = make_shared<Widget>("Shared-A");
    cout << "  p1 引用计数: " << p1.use_count() << endl;

    // 2. 拷贝 shared_ptr（引用计数增加）
    cout << "\n--- 拷贝 shared_ptr ---" << endl;
    auto p2 = p1;   // 引用计数 +1
    cout << "  p1 引用计数: " << p1.use_count() << endl;
    cout << "  p2 引用计数: " << p2.use_count() << endl;

    {
        auto p3 = p1;   // 引用计数 +1
        cout << "  进入内部作用域后:" << endl;
        cout << "  p1 引用计数: " << p1.use_count() << endl;
        cout << "  p3 引用计数: " << p3.use_count() << endl;
    }
    // p3 离开作用域，引用计数 -1
    cout << "  离开内部作用域后:" << endl;
    cout << "  p1 引用计数: " << p1.use_count() << endl;

    // 3. 移动 shared_ptr（引用计数不变）
    cout << "\n--- 移动 shared_ptr ---" << endl;
    auto p4 = std::move(p2);  // p2 变为 nullptr，引用计数不变
    cout << "  p1 引用计数: " << p1.use_count() << endl;
    cout << "  p2 是否为空: " << (p2 == nullptr ? "是" : "否") << endl;
    cout << "  p4 引用计数: " << p4.use_count() << endl;

    // 4. reset() 减少引用计数
    cout << "\n--- reset() 减少引用计数 ---" << endl;
    p4.reset();  // 引用计数 -1
    cout << "  p4 reset 后，p1 引用计数: " << p1.use_count() << endl;

    // 5. unique() 检查是否唯一持有
    cout << "\n--- unique() 检查 ---" << endl;
    cout << "  p1 是否唯一持有: " << (p1.unique() ? "是" : "否") << endl;

    // 6. shared_ptr 在容器中的使用
    cout << "\n--- shared_ptr 在 vector 中 ---" << endl;
    vector<shared_ptr<Widget>> vec;
    vec.push_back(make_shared<Widget>("Vec-1"));
    vec.push_back(make_shared<Widget>("Vec-2"));
    vec.push_back(make_shared<Widget>("Vec-3"));

    auto sp = vec[0];
    cout << "  vec[0] 引用计数: " << sp.use_count() << endl;

    cout << "\n--- shared_ptr 离开作用域自动释放 ---" << endl;
    // p1 离开作用域时，引用计数为 0，自动析构 Widget
}

// ============================================================
// 第三部分：weak_ptr —— 解决循环引用
// ============================================================

// 前向声明
class Student;
class Course;

// 学生类
class Student {
public:
    string name;
    // 学生选了哪些课（共享所有权）
    vector<shared_ptr<Course>> courses;

    explicit Student(const string& n) : name(n) {
        cout << "  Student 构造: " << name << endl;
    }

    ~Student() {
        cout << "  Student 析构: " << name << endl;
    }
};

// 课程类
class Course {
public:
    string title;
    // 课程有哪些学生（使用 weak_ptr 避免循环引用！）
    vector<weak_ptr<Student>> students;

    explicit Course(const string& t) : title(t) {
        cout << "  Course 构造: " << title << endl;
    }

    ~Course() {
        cout << "  Course 析构: " << title << endl;
    }

    // 打印选课学生（需要 lock() 提升为 shared_ptr）
    void printStudents() const {
        cout << "  课程 [" << title << "] 的学生: ";
        for (const auto& wp : students) {
            if (auto sp = wp.lock()) {   // lock() 提升为 shared_ptr
                cout << sp->name << " ";
            } else {
                cout << "(已释放) ";
            }
        }
        cout << endl;
    }
};

void demo_weak_ptr() {
    cout << "\n===== weak_ptr 演示 =====" << endl;

    // 1. weak_ptr 基本使用
    cout << "\n--- weak_ptr 基本使用 ---" << endl;
    auto sp = make_shared<Widget>("Weak-Test");
    weak_ptr<Widget> wp = sp;   // 从 shared_ptr 创建 weak_ptr

    cout << "  shared_ptr 引用计数: " << sp.use_count() << endl;
    cout << "  weak_ptr 是否过期: " << (wp.expired() ? "是" : "否") << endl;

    // 通过 lock() 访问对象
    if (auto locked = wp.lock()) {
        cout << "  lock() 成功，引用计数: " << locked.use_count() << endl;
        locked->show();
    }
    cout << "  lock() 后 shared_ptr 引用计数: " << sp.use_count() << endl;

    // 释放 shared_ptr
    cout << "\n--- 释放 shared_ptr 后 ---" << endl;
    sp.reset();
    cout << "  weak_ptr 是否过期: " << (wp.expired() ? "是" : "否") << endl;

    if (auto locked = wp.lock()) {
        cout << "  lock() 成功" << endl;
    } else {
        cout << "  lock() 失败：对象已被销毁" << endl;
    }

    // 2. 循环引用问题演示
    cout << "\n--- weak_ptr 解决循环引用 ---" << endl;
    {
        // 创建学生和课程
        auto student = make_shared<Student>("张三");
        auto course = make_shared<Course>("C++ 面试通关");

        // 学生选课（shared_ptr）
        student->courses.push_back(course);
        // 课程记录学生（weak_ptr —— 打破循环引用！）
        course->students.push_back(student);

        cout << "\n  选课关系建立完成" << endl;
        cout << "  Student 引用计数: " << student.use_count() << endl;
        cout << "  Course 引用计数: " << course.use_count() << endl;

        course->printStudents();

        cout << "\n  离开作用域..." << endl;
    }
    // Student 和 Course 都能被正确析构！
    // 因为 Course 中的 students 是 weak_ptr，不会阻止 Student 被释放
    cout << "  （Student 和 Course 都被正确析构，没有内存泄漏）" << endl;
}

// ============================================================
// 第四部分：自定义删除器
// ============================================================

void demo_custom_deleter() {
    cout << "\n===== 自定义删除器演示 =====" << endl;

    // 1. 使用 lambda 作为删除器
    cout << "\n--- lambda 删除器 ---" << endl;
    auto customDeleter = [](Widget* p) {
        cout << "  [自定义删除器] 即将删除: ";
        if (p) {
            p->show();
        }
        cout << endl;
        delete p;
    };

    // unique_ptr 的自定义删除器（删除器类型是模板参数的一部分）
    auto p1 = unique_ptr<Widget, decltype(customDeleter)>(
        new Widget("Custom-Unique"), customDeleter
    );

    // 2. shared_ptr 的自定义删除器（更灵活，删除器不是类型的一部分）
    cout << "\n--- shared_ptr 自定义删除器 ---" << endl;
    auto deleter = [](Widget* p) {
        cout << "  [shared_ptr 自定义删除器] 清理资源..." << endl;
        delete p;
    };

    shared_ptr<Widget> p2(new Widget("Custom-Shared"), deleter);
    cout << "  p2 引用计数: " << p2.use_count() << endl;

    // 3. 管理非 new 分配的资源（如 C 风格的文件句柄）
    cout << "\n--- 管理非 new 资源 ---" << endl;
    auto fileCloser = [](FILE* f) {
        if (f) {
            cout << "  [自定义删除器] 关闭文件" << endl;
            fclose(f);
        }
    };

    // 用 shared_ptr 管理 FILE*（注意：不能用 make_shared）
    shared_ptr<FILE> filePtr(fopen("/dev/null", "r"), fileCloser);
    if (filePtr) {
        cout << "  文件打开成功（/dev/null）" << endl;
    }
    // filePtr 离开作用域时，fileCloser 会被自动调用

    cout << "\n--- 自定义删除器离开作用域 ---" << endl;
}

// ============================================================
// main 函数
// ============================================================

int main() {
    cout << "========================================" << endl;
    cout << "  智能指针完整示例" << endl;
    cout << "  作者：青松与桑叶" << endl;
    cout << "========================================" << endl;

    demo_unique_ptr();
    demo_shared_ptr();
    demo_weak_ptr();
    demo_custom_deleter();

    cout << "\n========== 程序结束 ==========" << endl;
    return 0;
}
