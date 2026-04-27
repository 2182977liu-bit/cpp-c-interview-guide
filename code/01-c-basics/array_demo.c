/**
 * @file array_demo.c
 * @brief 数组与字符串示例代码
 * @author 青松与桑叶
 *
 * 编译方式：
 *   gcc -Wall -o array_demo array_demo.c && ./array_demo
 */

#include <stdio.h>
#include <string.h>

/* ==================== 1. 数组基本操作 ==================== */

void demo_array_basic() {
    printf("===== 1. 数组基本操作 =====\n");

    /* 定义和初始化 */
    int arr1[5] = {1, 2, 3, 4, 5};           /* 完全初始化 */
    int arr2[5] = {1, 2};                    /* 部分初始化，其余为0 */
    int arr3[] = {10, 20, 30};               /* 自动推断大小 */
    int arr4[5] = {0};                       /* 全部初始化为0 */
    (void)arr3; (void)arr4;  /* 避免未使用警告 */

    /* 计算数组元素个数 */
    int len = sizeof(arr1) / sizeof(arr1[0]);
    printf("arr1 有 %d 个元素\n", len);

    /* 遍历数组 */
    printf("arr1: ");
    for (int i = 0; i < len; i++) {
        printf("%d ", arr1[i]);
    }
    printf("\n");

    printf("arr2（部分初始化）: ");
    for (int i = 0; i < 5; i++) {
        printf("%d ", arr2[i]);  /* 输出: 1 2 0 0 0 */
    }
    printf("\n");

    /* 用指针遍历 */
    printf("用指针遍历 arr1: ");
    for (int *p = arr1; p < arr1 + len; p++) {
        printf("%d ", *p);
    }
    printf("\n");

    printf("\n");
}

/* ==================== 2. 数组名与指针 ==================== */

void demo_array_pointer() {
    printf("===== 2. 数组名与指针 =====\n");

    int arr[5] = {10, 20, 30, 40, 50};

    /* 数组名退化为指针 */
    int *p = arr;  /* 等价于 int *p = &arr[0] */

    printf("arr   = %p\n", (void *)arr);
    printf("&arr  = %p\n", (void *)&arr);
    printf("&arr[0] = %p\n", (void *)&arr[0]);
    printf("值都一样，但类型不同\n");

    /* sizeof 区别 */
    printf("sizeof(arr) = %lu 字节\n", sizeof(arr));  /* 20 */
    printf("sizeof(p)   = %lu 字节\n", sizeof(p));    /* 8 */

    /* 数组名是常量，不能赋值 */
    /* arr = p;  */  /* 编译错误！ */

    printf("\n");
}

/* ==================== 3. 二维数组 ==================== */

void demo_2d_array() {
    printf("===== 3. 二维数组 =====\n");

    /* 定义和初始化 */
    int matrix[2][3] = {
        {1, 2, 3},
        {4, 5, 6}
    };

    /* 下标访问 */
    printf("matrix[1][2] = %d\n", matrix[1][2]);  /* 6 */

    /* 用指针访问 */
    printf("*(*(matrix + 1) + 2) = %d\n", *(*(matrix + 1) + 2));  /* 6 */

    /* 遍历二维数组 */
    printf("二维数组内容:\n");
    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 3; j++) {
            printf("%d ", matrix[i][j]);
        }
        printf("\n");
    }

    /* 二维数组在内存中是连续的 */
    printf("\n内存中连续存储: ");
    int *p = &matrix[0][0];
    for (int i = 0; i < 6; i++) {
        printf("%d ", p[i]);
    }
    printf("\n");

    printf("\n");
}

/* ==================== 4. 字符串基础 ==================== */

void demo_string_basic() {
    printf("===== 4. 字符串基础 =====\n");

    /* 字符串的三种定义方式 */
    char str1[] = "hello";                      /* 字符数组，可修改 */
    char str2[] = {'h', 'e', 'l', 'l', 'o', '\0'};  /* 手动加 \0 */
    char *str3 = "hello";                       /* 指向字符串常量，不可修改 */

    printf("str1: %s (大小: %lu)\n", str1, sizeof(str1));  /* 大小为6（含\0） */
    printf("str2: %s (大小: %lu)\n", str2, sizeof(str2));  /* 大小为6 */
    printf("str3: %s\n", str3);

    /* str1 可以修改 */
    str1[0] = 'H';
    printf("修改后 str1: %s\n", str1);

    /* str3 不能修改！ */
    /* str3[0] = 'H';  */  /* 未定义行为！ */

    /* strlen vs sizeof */
    printf("\nstrlen(str1) = %lu (不包含 \\0)\n", strlen(str1));
    printf("sizeof(str1) = %lu (包含 \\0)\n", sizeof(str1));
    printf("sizeof(str3) = %lu (指针大小)\n", sizeof(str3));

    printf("\n");
}

/* ==================== 5. 字符串操作函数 ==================== */

/* 手写 strlen */
size_t my_strlen(const char *s) {
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}

/* 手写 strcpy */
char *my_strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++) != '\0') {
        /* 赋值在条件中完成 */
    }
    return ret;
}

/* 手写 strcmp */
int my_strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char *)s1 - *(unsigned char *)s2;
}

/* 手写 strcat */
char *my_strcat(char *dest, const char *src) {
    char *ret = dest;
    while (*dest) {
        dest++;  /* 找到 dest 的末尾 */
    }
    while ((*dest++ = *src++) != '\0') {
        /* 拼接 src */
    }
    return ret;
}

/* 手写 strncpy */
char *my_strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';  /* 剩余位置填 \0 */
    }
    return dest;
}

void demo_string_functions() {
    printf("===== 5. 字符串操作函数 =====\n");

    char src[] = "Hello";
    char dest[20] = "World";
    (void)dest;  /* 避免未使用警告 */

    /* strlen */
    printf("strlen(\"Hello\") = %lu\n", strlen(src));
    printf("my_strlen(\"Hello\") = %lu\n", my_strlen(src));

    /* strcpy */
    char buf1[20];
    my_strcpy(buf1, src);
    printf("strcpy: \"%s\"\n", buf1);

    /* strcmp */
    printf("strcmp(\"Hello\", \"Hello\") = %d\n", my_strcmp("Hello", "Hello"));
    printf("strcmp(\"Hello\", \"World\") = %d\n", my_strcmp("Hello", "World"));
    printf("strcmp(\"World\", \"Hello\") = %d\n", my_strcmp("World", "Hello"));

    /* strcat */
    char buf2[20] = "Hello, ";
    my_strcat(buf2, "World!");
    printf("strcat: \"%s\"\n", buf2);

    /* strncpy */
    char buf3[10];
    my_strncpy(buf3, "Hello, World!", 5);
    buf3[5] = '\0';  /* 手动添加 \0 */
    printf("strncpy(5): \"%s\"\n", buf3);

    /* 字符串比较不要用 == */
    char s1[] = "hello";
    char s2[] = "hello";
    if (s1 == s2) {
        printf("错误：用 == 比较的是地址\n");
    }
    if (my_strcmp(s1, s2) == 0) {
        printf("正确：用 strcmp 比较的是内容\n");
    }

    printf("\n");
}

/* ==================== 6. 字符串反转 ==================== */

void reverse_string(char *str) {
    if (str == NULL) return;
    int len = strlen(str);
    for (int i = 0; i < len / 2; i++) {
        char temp = str[i];
        str[i] = str[len - 1 - i];
        str[len - 1 - i] = temp;
    }
}

void demo_reverse() {
    printf("===== 6. 字符串反转 =====\n");

    char str[] = "Hello, World!";
    printf("反转前: %s\n", str);
    reverse_string(str);
    printf("反转后: %s\n", str);

    printf("\n");
}

/* ==================== main ==================== */

int main() {
    printf("========================================\n");
    printf("  数组与字符串 示例代码\n");
    printf("  作者：青松与桑叶\n");
    printf("========================================\n\n");

    demo_array_basic();
    demo_array_pointer();
    demo_2d_array();
    demo_string_basic();
    demo_string_functions();
    demo_reverse();

    printf("========================================\n");
    printf("  所有示例运行完毕！\n");
    printf("========================================\n");

    return 0;
}
