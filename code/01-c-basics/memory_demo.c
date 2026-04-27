/**
 * @file memory_demo.c
 * @brief 内存管理示例代码
 * @author 青松与桑叶
 *
 * 编译方式：
 *   gcc -Wall -o memory_demo memory_demo.c && ./memory_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================== 1. malloc / free 基本使用 ==================== */

void demo_malloc_free() {
    printf("===== 1. malloc / free 基本使用 =====\n");

    /* 分配单个 int */
    int *p = (int *)malloc(sizeof(int));
    if (p == NULL) {
        printf("内存分配失败！\n");
        return;
    }
    *p = 42;
    printf("*p = %d\n", *p);
    free(p);
    p = NULL;  /* 好习惯：释放后置空 */

    /* 分配数组 */
    int *arr = (int *)malloc(sizeof(int) * 5);
    if (arr == NULL) {
        printf("内存分配失败！\n");
        return;
    }
    for (int i = 0; i < 5; i++) {
        arr[i] = i * 10;
    }
    printf("数组: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");
    free(arr);
    arr = NULL;

    printf("\n");
}

/* ==================== 2. calloc 使用 ==================== */

void demo_calloc() {
    printf("===== 2. calloc 使用 =====\n");

    /* calloc 会自动初始化为 0 */
    int *p1 = (int *)malloc(sizeof(int) * 5);
    int *p2 = (int *)calloc(5, sizeof(int));

    printf("malloc 分配的值: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", p1[i]);  /* 可能是随机值 */
    }
    printf("（可能是随机值）\n");

    printf("calloc 分配的值: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", p2[i]);  /* 全部是 0 */
    }
    printf("（全部是0）\n");

    free(p1);
    free(p2);

    printf("\n");
}

/* ==================== 3. realloc 使用 ==================== */

void demo_realloc() {
    printf("===== 3. realloc 使用 =====\n");

    /* 先分配 5 个 int */
    int *arr = (int *)malloc(sizeof(int) * 5);
    if (arr == NULL) return;

    for (int i = 0; i < 5; i++) {
        arr[i] = i + 1;
    }
    printf("原始数组(5个): ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    /* 扩展到 10 个 */
    /* 注意：用临时变量接收 realloc 的返回值 */
    int *new_arr = (int *)realloc(arr, sizeof(int) * 10);
    if (new_arr == NULL) {
        /* 扩展失败，原来的 arr 仍然有效 */
        printf("realloc 失败！\n");
        free(arr);
        return;
    }
    arr = new_arr;

    /* 新增的元素初始化 */
    for (int i = 5; i < 10; i++) {
        arr[i] = i + 1;
    }

    printf("扩展后数组(10个): ");
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    free(arr);
    arr = NULL;

    printf("\n");
}

/* ==================== 4. 内存泄漏示例 ==================== */

void leak_example() {
    /* 这个函数演示了内存泄漏 */
    int *p = (int *)malloc(sizeof(int));
    *p = 42;
    /* 忘记 free(p) 了！这就是内存泄漏 */
    /* 在实际代码中，应该在这里加上 free(p); */
}

void no_leak_example() {
    /* 正确的做法 */
    int *p = (int *)malloc(sizeof(int));
    if (p == NULL) return;
    *p = 42;
    free(p);
    p = NULL;
}

/* 演示 goto 统一清理（C语言中 goto 的合法用法） */
int goto_cleanup_demo() {
    printf("===== 4. 内存泄漏与 goto 清理 =====\n");

    int *p1 = NULL;
    int *p2 = NULL;
    int *p3 = NULL;

    p1 = (int *)malloc(sizeof(int));
    if (p1 == NULL) goto cleanup;
    *p1 = 1;

    p2 = (int *)malloc(sizeof(int));
    if (p2 == NULL) goto cleanup;
    *p2 = 2;

    p3 = (int *)malloc(sizeof(int));
    if (p3 == NULL) goto cleanup;
    *p3 = 3;

    printf("分配成功: p1=%d, p2=%d, p3=%d\n", *p1, *p2, *p3);

cleanup:
    /* 统一释放所有已分配的内存 */
    /* free(NULL) 是安全的，所以不用担心某个指针还是 NULL */
    free(p1);
    free(p2);
    free(p3);
    printf("已统一清理所有内存\n");

    printf("\n");
    return 0;
}

/* ==================== 5. 内存对齐演示 ==================== */

void demo_memory_alignment() {
    printf("===== 5. 内存对齐演示 =====\n");

    /* 示例1：基本类型对齐 */
    printf("基本类型大小:\n");
    printf("  sizeof(char)   = %lu\n", sizeof(char));
    printf("  sizeof(short)  = %lu\n", sizeof(short));
    printf("  sizeof(int)    = %lu\n", sizeof(int));
    printf("  sizeof(long)   = %lu\n", sizeof(long));
    printf("  sizeof(float)  = %lu\n", sizeof(float));
    printf("  sizeof(double) = %lu\n", sizeof(double));
    printf("  sizeof(char*)  = %lu\n", sizeof(char *));

    /* 示例2：结构体对齐 */
    struct Example1 {
        char a;
        int b;
        short c;
    };

    struct Example2 {
        char a;
        char b;
        int c;
        short d;
    };

    struct Optimized {
        int c;       /* 4字节，对齐4 */
        short d;     /* 2字节，对齐2 */
        char a;      /* 1字节，对齐1 */
        char b;      /* 1字节，对齐1 */
    };

    printf("\n结构体大小（注意填充）:\n");
    printf("  Example1 (char, int, short):     %lu 字节\n", sizeof(struct Example1));
    printf("  Example2 (char, char, int, short): %lu 字节\n", sizeof(struct Example2));
    printf("  Optimized (优化排列):            %lu 字节\n", sizeof(struct Optimized));

    /* 示例3：修改对齐值 */
    #pragma pack(push, 1)
    struct Packed {
        char a;
        int b;
        short c;
    };
    #pragma pack(pop)

    printf("  Packed (1字节对齐):              %lu 字节\n", sizeof(struct Packed));

    /* 示例4：结构体成员偏移量 */
    printf("\n成员偏移量:\n");
    printf("  Example1.a 偏移: %lu\n", (size_t)&((struct Example1 *)0)->a);
    printf("  Example1.b 偏移: %lu\n", (size_t)&((struct Example1 *)0)->b);
    printf("  Example1.c 偏移: %lu\n", (size_t)&((struct Example1 *)0)->c);

    printf("\n");
}

/* ==================== 6. 栈与堆对比 ==================== */

void stack_demo() {
    printf("===== 6. 栈与堆对比 =====\n");

    /* 栈上分配 */
    int stack_var = 10;
    int stack_arr[100];

    printf("栈变量地址: %p\n", (void *)&stack_var);
    printf("栈数组地址: %p\n", (void *)stack_arr);

    /* 堆上分配 */
    int *heap_var = (int *)malloc(sizeof(int));
    int *heap_arr = (int *)malloc(sizeof(int) * 100);

    printf("堆变量地址: %p\n", (void *)heap_var);
    printf("堆数组地址: %p\n", (void *)heap_arr);

    /* 通常栈地址比堆地址大（栈从高地址向低地址增长） */
    printf("\n注意：栈地址通常 > 堆地址\n");
    printf("  栈: %p\n", (void *)&stack_var);
    printf("  堆: %p\n", (void *)heap_var);

    free(heap_var);
    free(heap_arr);

    printf("\n");
}

/* ==================== main ==================== */

int main() {
    printf("========================================\n");
    printf("  内存管理 示例代码\n");
    printf("  作者：青松与桑叶\n");
    printf("========================================\n\n");

    demo_malloc_free();
    demo_calloc();
    demo_realloc();
    goto_cleanup_demo();
    demo_memory_alignment();
    stack_demo();

    printf("========================================\n");
    printf("  所有示例运行完毕！\n");
    printf("========================================\n");

    return 0;
}
