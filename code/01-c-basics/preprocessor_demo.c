/**
 * @file preprocessor_demo.c
 * @brief 预处理与编译链接示例代码
 * @author 青松与桑叶
 *
 * 编译方式：
 *   gcc -Wall -o preprocessor_demo preprocessor_demo.c && ./preprocessor_demo
 *
 * 查看预处理结果：
 *   gcc -E preprocessor_demo.c -o preprocessor_demo.i
 */

#include <stdio.h>
#include <string.h>

/* ==================== 1. 宏定义 ==================== */

/* 对象宏（无参数） */
#define PI 3.14159265
#define MAX_SIZE 100
#define VERSION "1.0.0"

/* 函数宏（带参数） */
/* 注意：参数和整体都要加括号！ */
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))
#define ABS(x) ((x) >= 0 ? (x) : -(x))

/* 字符串化宏 */
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

/* 连接宏 */
#define CONCAT(a, b) a##b

/* 代码块宏：用 do { } while(0) 包裹 */
#define SWAP(a, b) do {    \
    int temp = a;          \
    a = b;                 \
    b = temp;              \
} while(0)

void demo_macro() {
    printf("===== 1. 宏定义 =====\n");

    /* 对象宏 */
    printf("PI = %f\n", PI);
    printf("MAX_SIZE = %d\n", MAX_SIZE);
    printf("VERSION = %s\n", VERSION);

    /* 函数宏 */
    printf("MAX(3, 5) = %d\n", MAX(3, 5));
    printf("MIN(3, 5) = %d\n", MIN(3, 5));
    printf("SQUARE(4) = %d\n", SQUARE(4));
    printf("SQUARE(3+1) = %d\n", SQUARE(3 + 1));  /* ((3+1) * (3+1)) = 16 */
    printf("ABS(-5) = %d\n", ABS(-5));

    /* 字符串化 */
    printf("STRINGIFY(hello) = %s\n", STRINGIFY(hello));
    printf("TOSTRING(100) = %s\n", TOSTRING(100));

    /* 连接 */
    int CONCAT(var, 1) = 10;  /* 生成 var1 */
    int CONCAT(var, 2) = 20;  /* 生成 var2 */
    printf("var1 = %d, var2 = %d\n", var1, var2);

    /* 代码块宏 */
    int x = 3, y = 7;
    printf("交换前: x=%d, y=%d\n", x, y);
    SWAP(x, y);
    printf("交换后: x=%d, y=%d\n", x, y);

    printf("\n");
}

/* ==================== 2. 条件编译 ==================== */

/* 定义调试开关 */
#define DEBUG
/* #define VERBOSE */

void demo_conditional_compilation() {
    printf("===== 2. 条件编译 =====\n");

    /* 基本条件编译 */
#ifdef DEBUG
    printf("调试模式已开启\n");
#endif

#ifndef RELEASE
    printf("当前不是发布版本\n");
#endif

    /* #if / #elif / #else */
#define LEVEL 2

#if LEVEL == 1
    printf("级别 1：基本信息\n");
#elif LEVEL == 2
    printf("级别 2：详细信息\n");
#elif LEVEL == 3
    printf("级别 3：全部信息\n");
#else
    printf("未知级别\n");
#endif

    /* defined 运算符 */
#if defined(DEBUG) && defined(VERBOSE)
    printf("调试 + 详细模式\n");
#elif defined(DEBUG)
    printf("调试模式（非详细）\n");
#else
    printf("普通模式\n");
#endif

    /* 取消宏定义 */
#define TEMP_MACRO 100
#ifdef TEMP_MACRO
    printf("TEMP_MACRO 已定义，值为 %d\n", TEMP_MACRO);
#endif
#undef TEMP_MACRO
#ifdef TEMP_MACRO
    printf("这行不会执行\n");
#else
    printf("TEMP_MACRO 已取消定义\n");
#endif

    printf("\n");
}

/* ==================== 3. 预定义宏 ==================== */

void demo_predefined_macros() {
    printf("===== 3. 预定义宏 =====\n");

    printf("当前文件:   %s\n", __FILE__);
    printf("当前行号:   %d\n", __LINE__);
    printf("编译日期:   %s\n", __DATE__);
    printf("编译时间:   %s\n", __TIME__);
    printf("是否标准C:  %d\n", __STDC__);

    /* C99 新增的 __func__ */
    printf("当前函数:   %s\n", __func__);

    printf("\n");
}

/* ==================== 4. 宏定义日志系统 ==================== */

/* 日志宏示例 */
#define LOG_INFO(fmt, ...) \
    printf("[INFO] %s:%d %s(): " fmt "\n", \
           __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    printf("[WARN] %s:%d %s(): " fmt "\n", \
           __FILE__, __LINE__, __func__, ##__VA_ARGS__)

#ifdef DEBUG
    #define LOG_DEBUG(fmt, ...) \
        printf("[DEBUG] %s:%d %s(): " fmt "\n", \
               __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(fmt, ...)  /* 调试日志在非调试模式下为空 */
#endif

void demo_log_macros() {
    printf("===== 4. 宏定义日志系统 =====\n");

    LOG_INFO("程序启动");
    LOG_INFO("版本号: %s", VERSION);
    LOG_WARN("这是一个警告消息");
    LOG_DEBUG("调试信息: x = %d, y = %d", 42, 99);

    printf("\n");
}

/* ==================== 5. 头文件保护演示 ==================== */

/* 模拟头文件保护 */
#ifndef MY_DEMO_H
#define MY_DEMO_H

/* 这个条件确保内容只被处理一次 */
#define DEMO_VALUE 12345

#endif /* MY_DEMO_H */

void demo_header_guard() {
    printf("===== 5. 头文件保护 =====\n");

    /* 即使多次 include，DEMO_VALUE 也只定义一次 */
    printf("DEMO_VALUE = %d\n", DEMO_VALUE);

    printf("\n");
}

/* ==================== 6. 宏 vs typedef ==================== */

void demo_macro_vs_typedef() {
    printf("===== 6. 宏 vs typedef =====\n");

    /* #define 定义类型别名 */
#define INT_PTR_DEFINE int *

    /* typedef 定义类型别名 */
    typedef int *INT_PTR_TYPEDEF;

    INT_PTR_DEFINE a, b;    /* a 是 int*，b 是 int（宏展开后: int *a, b） */
    (void)b;  /* 避免未使用警告 */
    INT_PTR_TYPEDEF c, d;   /* c 和 d 都是 int* */

    int val = 42;
    a = &val;
    c = &val;
    /* b = &val;  */  /* 编译警告：b 是 int，不是 int* */
    d = &val;

    printf("a 指向的值: %d\n", *a);
    printf("c 指向的值: %d\n", *c);
    printf("d 指向的值: %d\n", *d);

    printf("\n推荐：类型别名用 typedef，不要用 #define\n");

    printf("\n");
}

/* ==================== 7. 常用编译器指令 ==================== */

void demo_pragma() {
    printf("===== 7. 编译器指令 =====\n");

    /* #pragma pack 演示 */
    struct Normal {
        char a;    /* 1 + 3填充 */
        int b;     /* 4 */
    };

    #pragma pack(push, 1)
    struct Packed {
        char a;    /* 1 */
        int b;     /* 4 */
    };
    #pragma pack(pop)

    printf("Normal 结构体大小: %lu 字节\n", sizeof(struct Normal));
    printf("Packed 结构体大小: %lu 字节\n", sizeof(struct Packed));

    /* #pragma message（编译时输出信息） */
    /* #pragma message("编译信息：正在编译预处理示例") */

    printf("\n");
}

/* ==================== main ==================== */

int main() {
    printf("========================================\n");
    printf("  预处理与编译链接 示例代码\n");
    printf("  作者：青松与桑叶\n");
    printf("========================================\n\n");

    demo_macro();
    demo_conditional_compilation();
    demo_predefined_macros();
    demo_log_macros();
    demo_header_guard();
    demo_macro_vs_typedef();
    demo_pragma();

    printf("========================================\n");
    printf("  所有示例运行完毕！\n");
    printf("========================================\n");

    return 0;
}
