/**
 * @file virtual_demo.cpp
 * @brief 2.5 虚函数深度剖析 —— 完整示例代码
 * @author 青松与桑叶
 *
 * 编译: g++ -std=c++17 -Wall -o virtual_demo virtual_demo.cpp
 * 运行: ./virtual_demo
 *
 * 注意：本文件使用函数指针打印 vtable 地址，属于底层实现探索。
 * 不同编译器和编译选项可能导致输出不同。
 */

#include <iostream>
#include <string>
#include <cstdint>
using namespace std;

// ============================================================
// 辅助函数：打印 vtable 信息
// ============================================================

// 虚函数指针类型
using VFunc = void(*)();

/**
 * @brief 打印对象的 vtable 地址和虚函数指针
 * @param obj 对象指针
 * @param className 类名（用于显示）
 * @param vfuncCount 虚函数数量
 */
void printVtable(void* obj, const string& className, int vfuncCount) {
    // 读取对象前 8 字节（vptr）
    uintptr_t vptr = *reinterpret_cast<uintptr_t*>(obj);
    cout << "  " << className << " 对象地址: " << obj << endl;
    cout << "  " << className << " vptr 地址: " << reinterpret_cast<void*>(vptr) << endl;

    // 读取 vtable 中的函数指针
    auto vtable = reinterpret_cast<uintptr_t*>(vptr);
    for (int i = 0; i < vfuncCount; i++) {
        cout << "  vtable[" << i << "] = " << reinterpret_cast<void*>(vtable[i]) << endl;
    }
    cout << endl;
}

// ============================================================
// 第一部分：单继承下的 vtable 布局
// ============================================================

class Base1 {
public:
    int baseData = 100;

    virtual void func1() {
        cout << "  Base1::func1() 被调用" << endl;
    }

    virtual void func2() {
        cout << "  Base1::func2() 被调用" << endl;
    }

    virtual ~Base1() {
        cout << "  Base1 析构" << endl;
    }
};

class Derived1 : public Base1 {
public:
    int derivedData = 200;

    // 重写 func1
    void func1() override {
        cout << "  Derived1::func1() 被调用" << endl;
    }

    // 新增虚函数 func3
    virtual void func3() {
        cout << "  Derived1::func3() 被调用" << endl;
    }

    ~Derived1() override {
        cout << "  Derived1 析构" << endl;
    }
};

void demo_single_inheritance_vtable() {
    cout << "===== 单继承下的 vtable 布局 =====" << endl;

    cout << "\n--- Base1 的 vtable ---" << endl;
    Base1 b1;
    // Base1 的 vtable 有 3 个条目：func1, func2, ~Base1
    printVtable(&b1, "Base1", 3);

    cout << "--- Derived1 的 vtable ---" << endl;
    Derived1 d1;
    // Derived1 的 vtable 有 4 个条目：func1(重写), func2(继承), ~Derived1, func3(新增)
    printVtable(&d1, "Derived1", 4);

    // 验证：通过基类指针调用虚函数
    cout << "--- 通过基类指针调用虚函数 ---" << endl;
    Base1* ptr = &d1;
    ptr->func1();   // 应该调用 Derived1::func1()
    ptr->func2();   // 应该调用 Base1::func2()

    // 对象大小对比
    cout << "\n--- 对象大小 ---" << endl;
    cout << "  sizeof(Base1):    " << sizeof(Base1) << " 字节" << endl;
    cout << "  sizeof(Derived1): " << sizeof(Derived1) << " 字节" << endl;
    cout << "  （vptr 8 字节 + baseData 4 字节 + 对齐 + derivedData 4 字节）" << endl;
}

// ============================================================
// 第二部分：多重继承下的 vptr
// ============================================================

class BaseA {
public:
    int dataA = 1;

    virtual void funcA() {
        cout << "  BaseA::funcA() 被调用" << endl;
    }

    virtual ~BaseA() {
        cout << "  BaseA 析构" << endl;
    }
};

class BaseB {
public:
    int dataB = 2;

    virtual void funcB() {
        cout << "  BaseB::funcB() 被调用" << endl;
    }

    virtual ~BaseB() {
        cout << "  BaseB 析构" << endl;
    }
};

class MultiDerived : public BaseA, public BaseB {
public:
    int dataC = 3;

    void funcA() override {
        cout << "  MultiDerived::funcA() 被调用" << endl;
    }

    void funcB() override {
        cout << "  MultiDerived::funcB() 被调用" << endl;
    }

    ~MultiDerived() override {
        cout << "  MultiDerived 析构" << endl;
    }
};

void demo_multiple_inheritance_vptr() {
    cout << "\n===== 多重继承下的 vptr =====" << endl;

    MultiDerived md;

    cout << "--- 对象大小 ---" << endl;
    cout << "  sizeof(MultiDerived): " << sizeof(MultiDerived) << " 字节" << endl;

    cout << "\n--- BaseA 部分的 vtable ---" << endl;
    BaseA* pA = &md;
    cout << "  pA (BaseA*) 地址: " << pA << endl;
    printVtable(pA, "BaseA", 2);  // funcA, ~BaseA

    cout << "--- BaseB 部分的 vtable ---" << endl;
    BaseB* pB = &md;
    cout << "  pB (BaseB*) 地址: " << pB << endl;
    cout << "  地址偏移: " << (reinterpret_cast<char*>(pB) - reinterpret_cast<char*>(pA))
         << " 字节" << endl;
    printVtable(pB, "BaseB", 2);  // funcB, ~BaseB

    // 验证：通过不同基类指针调用虚函数
    cout << "--- 通过不同基类指针调用虚函数 ---" << endl;
    pA->funcA();   // MultiDerived::funcA()
    pB->funcB();   // MultiDerived::funcB()

    // 验证：指针转换时的地址调整
    cout << "\n--- 指针转换时的地址调整 ---" << endl;
    cout << "  &md          = " << &md << endl;
    cout << "  (BaseA*)&md  = " << static_cast<BaseA*>(&md) << endl;
    cout << "  (BaseB*)&md  = " << static_cast<BaseB*>(&md) << endl;
    cout << "  注意：BaseB* 的地址与对象起始地址不同！" << endl;
}

// ============================================================
// 第三部分：虚析构函数的必要性
// ============================================================

class NoVirtualDestructor {
public:
    ~NoVirtualDestructor() {   // 不是虚析构函数！
        cout << "  NoVirtualDestructor 析构" << endl;
    }
};

class DerivedNoVirtual : public NoVirtualDestructor {
public:
    ~DerivedNoVirtual() {
        cout << "  DerivedNoVirtual 析构" << endl;
    }
};

class WithVirtualDestructor {
public:
    virtual ~WithVirtualDestructor() {   // 虚析构函数
        cout << "  WithVirtualDestructor 析构" << endl;
    }
};

class DerivedWithVirtual : public WithVirtualDestructor {
public:
    ~DerivedWithVirtual() override {
        cout << "  DerivedWithVirtual 析构" << endl;
    }
};

void demo_virtual_destructor() {
    cout << "\n===== 虚析构函数的必要性 =====" << endl;

    // 1. 非虚析构函数的问题
    cout << "\n--- 非虚析构函数（有问题）---" << endl;
    cout << "  通过基类指针 delete 派生类对象:" << endl;
    NoVirtualDestructor* p1 = new DerivedNoVirtual();
    delete p1;
    // 注意：只调用了 NoVirtualDestructor 析构！
    // DerivedNoVirtual 析构没有被调用 → 资源泄漏！

    // 2. 虚析构函数的正确行为
    cout << "\n--- 虚析构函数（正确）---" << endl;
    cout << "  通过基类指针 delete 派生类对象:" << endl;
    WithVirtualDestructor* p2 = new DerivedWithVirtual();
    delete p2;
    // 正确：先调用 DerivedWithVirtual 析构，再调用 WithVirtualDestructor 析构

    cout << "\n--- 总结 ---" << endl;
    cout << "  非虚析构：只调用基类析构，派生类资源泄漏" << endl;
    cout << "  虚析构：先派生类析构，再基类析构，资源正确释放" << endl;
}

// ============================================================
// 第四部分：构造/析构中的虚函数调用
// ============================================================

class BaseVFunc {
public:
    BaseVFunc() {
        cout << "  BaseVFunc 构造开始" << endl;
        virtualFunc();    // 在构造函数中调用虚函数
        cout << "  BaseVFunc 构造结束" << endl;
    }

    virtual void virtualFunc() const {
        cout << "  BaseVFunc::virtualFunc() —— 在构造函数中被调用" << endl;
    }

    virtual ~BaseVFunc() {
        cout << "  BaseVFunc 析构开始" << endl;
        virtualFunc();    // 在析构函数中调用虚函数
        cout << "  BaseVFunc 析构结束" << endl;
    }
};

class DerivedVFunc : public BaseVFunc {
public:
    DerivedVFunc() {
        cout << "  DerivedVFunc 构造开始" << endl;
        virtualFunc();
        cout << "  DerivedVFunc 构造结束" << endl;
    }

    void virtualFunc() const override {
        cout << "  DerivedVFunc::virtualFunc()" << endl;
    }

    ~DerivedVFunc() override {
        cout << "  DerivedVFunc 析构开始" << endl;
        virtualFunc();
        cout << "  DerivedVFunc 析构结束" << endl;
    }
};

void demo_virtual_in_ctor_dtor() {
    cout << "\n===== 构造/析构中的虚函数调用 =====" << endl;

    cout << "\n--- 创建 DerivedVFunc 对象 ---" << endl;
    DerivedVFunc d;

    cout << "\n--- 销毁 DerivedVFunc 对象 ---" << endl;
    // d 离开作用域

    cout << "\n--- 分析 ---" << endl;
    cout << "  在基类构造函数中调用虚函数 → 调用基类版本（不是派生类版本）" << endl;
    cout << "  在基类析构函数中调用虚函数 → 调用基类版本（不是派生类版本）" << endl;
    cout << "  原因：构造基类部分时，派生类部分尚未构造，vptr 指向基类 vtable" << endl;
    cout << "        析构基类部分时，派生类部分已经析构，vptr 指向基类 vtable" << endl;
}

// ============================================================
// main 函数
// ============================================================

int main() {
    cout << "========================================" << endl;
    cout << "  虚函数深度剖析 —— 完整示例" << endl;
    cout << "  作者：青松与桑叶" << endl;
    cout << "========================================" << endl;

    demo_single_inheritance_vtable();
    demo_multiple_inheritance_vptr();
    demo_virtual_destructor();
    demo_virtual_in_ctor_dtor();

    cout << "\n========== 程序结束 ==========" << endl;
    return 0;
}
