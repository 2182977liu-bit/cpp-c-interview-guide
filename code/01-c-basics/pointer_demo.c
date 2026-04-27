/**
 * @file pointer_demo.c
 * @brief 指针与引用示例代码
 * @author 青松与桑叶
 *
 * 编译方式：
 *   C语言: gcc -Wall -o pointer_demo pointer_demo.c && ./pointer_demo
 *   C++:   g++ -std=c++17 -Wall -o pointer_demo pointer_demo.c && ./pointer_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 1. 基本指针操作 ==================== */

void demo_basic_pointer() {
    printf("===== 1. 基本指针操作 =====\n");

    int a = 42;           /* 定义一个整型变量 */
    int *p = &a;          /* 定义指针p，指向a的地址 */

    printf("a 的值:       %d\n", a);
    printf("a 的地址:     %p\n", (void *)&a);
    printf("p 的值:       %p\n", (void *)p);
    printf("p 指向的值:   %d\n", *p);       /* 通过解引用访问值 */
    printf("p 的地址:     %p\n", (void *)&p); /* 指针本身也有地址 */

    /* 通过指针修改值 */
    *p = 100;
    printf("修改后 a = %d\n", a);  /* a 被修改为 100 */

    /* NULL 指针 */
    int *null_p = NULL;
    if (null_p == NULL) {
        printf("null_p 是空指针\n");
    }

    printf("\n");
}

/* ==================== 2. 指针运算 ==================== */

void demo_pointer_arithmetic() {
    printf("===== 2. 指针运算 =====\n");

    int arr[5] = {10, 20, 30, 40, 50};
    int *p = arr;  /* 数组名退化为指向首元素的指针 */

    printf("数组元素: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", *(p + i));  /* 指针+整数，偏移 i 个元素 */
    }
    printf("\n");

    /* 指针自增 */
    printf("用指针自增遍历: ");
    for (int *q = arr; q < arr + 5; q++) {
        printf("%d ", *q);
    }
    printf("\n");

    /* 指针减法：计算两个指针之间的元素个数 */
    int *start = arr;
    int *end = arr + 4;
    printf("start 和 end 之间相隔 %ld 个元素\n", (long)(end - start));

    printf("\n");
}

/* ==================== 3. 多级指针 ==================== */

void demo_multi_pointer() {
    printf("===== 3. 多级指针 =====\n");

    int a = 42;
    int *p = &a;       /* 一级指针 */
    int **pp = &p;     /* 二级指针 */
    int ***ppp = &pp;  /* 三级指针 */

    printf("a    = %d\n", a);
    printf("*p   = %d\n", *p);
    printf("**pp = %d\n", **pp);
    printf("***ppp = %d\n", ***ppp);

    /* 二级指针的经典用途：在函数中修改指针 */
    printf("\n--- 在函数中分配内存 ---\n");
    int *dynamic = NULL;
    /* 通过二级指针分配内存 */
    int **pp_dynamic = &dynamic;
    *pp_dynamic = (int *)malloc(sizeof(int));
    **pp_dynamic = 99;
    printf("通过二级指针分配的值: %d\n", *dynamic);
    free(dynamic);
    dynamic = NULL;

    printf("\n");
}

/* ==================== 4. 函数指针 ==================== */

/* 定义几个简单的数学函数 */
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }
int mul(int a, int b) { return a * b; }
int int_div(int a, int b) { return b != 0 ? a / b : 0; }

void demo_function_pointer() {
    printf("===== 4. 函数指针 =====\n");

    /* 基本用法：定义函数指针并调用 */
    int (*fp)(int, int) = add;  /* fp 指向 add 函数 */
    printf("add(3, 5) = %d\n", fp(3, 5));

    /* 用 typedef 简化 */
    typedef int (*MathOp)(int, int);
    MathOp op = sub;
    printf("sub(10, 3) = %d\n", op(10, 3));

    /* 函数指针数组 */
    printf("\n--- 函数指针数组 ---\n");
    MathOp ops[4] = {add, sub, mul, int_div};
    const char *names[4] = {"加", "减", "乘", "除"};

    for (int i = 0; i < 4; i++) {
        printf("10 %s 3 = %d\n", names[i], ops[i](10, 3));
    }

    /* 回调函数示例 */
    printf("\n--- 回调函数 ---\n");
    /* 定义一个接受函数指针的函数 */
    void apply(int a, int b, MathOp operation, const char *name) {
        printf("%s(%d, %d) = %d\n", name, a, b, operation(a, b));
    }
    apply(8, 2, mul, "mul");
    apply(8, 2, int_div, "div");

    printf("\n");
}

/* ==================== 5. const 指针 ==================== */

void demo_const_pointer() {
    printf("===== 5. const 指针 =====\n");

    int x = 10, y = 20;

    /* const 在 * 左边：指向的值不能改，但指针可以改指向 */
    const int *p1 = &x;
    /* *p1 = 30;  */  /* 错误！不能通过 p1 修改值 */
    p1 = &y;          /* 正确！可以改指向 */
    printf("p1 指向 y = %d\n", *p1);

    /* const 在 * 右边：指针不能改指向，但值可以改 */
    int *const p2 = &x;
    *p2 = 30;         /* 正确！可以通过 p2 修改值 */
    /* p2 = &y;  */    /* 错误！不能改指向 */
    printf("x = %d\n", x);

    /* 两边都 const */
    const int *const p3 = &x;
    /* *p3 = 40;  */  /* 错误！ */
    /* p3 = &y;  */    /* 错误！ */
    (void)p3;  /* 避免未使用警告 */

    printf("\n");
}

/* ==================== 6. 指针与数组名区别 ==================== */

void demo_array_vs_pointer() {
    printf("===== 6. 数组名与指针的区别 =====\n");

    int arr[5] = {1, 2, 3, 4, 5};
    int *p = arr;

    /* 区别1：sizeof 不同 */
    printf("sizeof(arr) = %lu\n", sizeof(arr));  /* 20（整个数组） */
    printf("sizeof(p)   = %lu\n", sizeof(p));    /* 8（指针本身） */

    /* 区别2：数组名不能被赋值 */
    /* arr = p;  */  /* 编译错误！ */
    p = arr;        /* 正确 */

    /* 区别3：&arr 和 &arr[0] 类型不同 */
    printf("&arr    = %p (类型: int (*)[5])\n", (void *)&arr);
    printf("&arr[0] = %p (类型: int *)\n", (void *)&arr[0]);
    printf("值相同，但类型不同\n");

    printf("\n");
}

/* ==================== 7. C++ 引用（条件编译） ==================== */

#ifdef __cplusplus
/**
 * C++ 中的引用示例
 * 注意：这部分代码只在 C++ 编译器下编译
 */
void demo_reference() {
    printf("===== 7. 引用（C++） =====\n");

    int a = 10;
    int &ref = a;   /* ref 是 a 的引用（别名） */

    printf("a   = %d\n", a);
    printf("ref = %d\n", ref);

    ref = 20;
    printf("修改 ref 后, a = %d\n", a);  /* a 也变成了 20 */

    /* 引用必须在声明时初始化 */
    /* int &ref2;  */  /* 编译错误！ */

    /* 引用不能重新绑定 */
    int b = 30;
    ref = b;   /* 这不是让 ref 绑定到 b，而是把 b 的值赋给 a */
    printf("ref = b 后, a = %d, b = %d\n", a, b);

    /* 指针 vs 引用：交换函数对比 */
    auto swap_ptr = [](int *x, int *y) {
        int temp = *x;
        *x = *y;
        *y = temp;
    };

    auto swap_ref = [](int &x, int &y) {
        int temp = x;
        x = y;
        y = temp;
    };

    int x = 1, y = 2;
    swap_ptr(&x, &y);
    printf("swap_ptr: x=%d, y=%d\n", x, y);

    swap_ref(x, y);
    printf("swap_ref: x=%d, y=%d\n", x, y);

    printf("\n");
}
#endif /* __cplusplus */

/* ==================== main ==================== */

int main() {
    printf("========================================\n");
    printf("  指针与引用 示例代码\n");
    printf("  作者：青松与桑叶\n");
    printf("========================================\n\n");

    demo_basic_pointer();
    demo_pointer_arithmetic();
    demo_multi_pointer();
    demo_function_pointer();
    demo_const_pointer();
    demo_array_vs_pointer();

#ifdef __cplusplus
    demo_reference();
#else
    printf("提示：使用 g++ 编译可以看到引用（C++）的示例\n");
#endif

    printf("========================================\n");
    printf("  所有示例运行完毕！\n");
    printf("========================================\n");

    return 0;
}
