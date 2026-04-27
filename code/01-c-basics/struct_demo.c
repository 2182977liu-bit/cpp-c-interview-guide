/**
 * @file struct_demo.c
 * @brief struct 与 union 示例代码
 * @author 青松与桑叶
 *
 * 编译方式：
 *   gcc -Wall -o struct_demo struct_demo.c && ./struct_demo
 */

#include <stdio.h>
#include <string.h>

/* ==================== 1. 结构体基本用法 ==================== */

/* 定义学生结构体 */
typedef struct {
    char name[20];
    int age;
    float score;
} Student;

void demo_struct_basic() {
    printf("===== 1. 结构体基本用法 =====\n");

    /* 初始化结构体 */
    Student s1 = {"张三", 20, 95.5f};
    Student s2;
    s2.age = 21;
    s2.score = 88.0f;
    strncpy(s2.name, "李四", sizeof(s2.name) - 1);
    s2.name[sizeof(s2.name) - 1] = '\0';

    /* 访问成员 */
    printf("姓名: %s, 年龄: %d, 成绩: %.1f\n", s1.name, s1.age, s1.score);
    printf("姓名: %s, 年龄: %d, 成绩: %.1f\n", s2.name, s2.age, s2.score);

    /* 结构体指针 */
    Student *p = &s1;
    printf("通过指针访问: %s, %d, %.1f\n", p->name, p->age, p->score);
    /* p->name 等价于 (*p).name */

    /* 结构体赋值（值拷贝） */
    Student s3 = s1;
    printf("结构体拷贝: %s, %d, %.1f\n", s3.name, s3.age, s3.score);

    printf("\n");
}

/* ==================== 2. 结构体内存对齐 ==================== */

void demo_struct_alignment() {
    printf("===== 2. 结构体内存对齐 =====\n");

    /* 示例1：未优化的结构体 */
    struct BadLayout {
        char a;      /* 1字节 */
        /* 3字节填充 */
        double b;    /* 8字节 */
        char c;      /* 1字节 */
        /* 7字节填充 */
    };

    /* 示例2：优化后的结构体 */
    struct GoodLayout {
        double b;    /* 8字节 */
        char a;      /* 1字节 */
        char c;      /* 1字节 */
        /* 6字节填充 */
    };

    printf("BadLayout  (char, double, char): %lu 字节\n", sizeof(struct BadLayout));
    printf("GoodLayout (double, char, char): %lu 字节\n", sizeof(struct GoodLayout));
    printf("优化后节省了 %lu 字节\n",
           sizeof(struct BadLayout) - sizeof(struct GoodLayout));

    /* 示例3：各种组合 */
    struct Test1 { char a; int b; short c; };
    struct Test2 { char a; char b; int c; };
    struct Test3 { int a; char b; short c; };
    struct Test4 { short a; char b; int c; };

    printf("\n不同排列的结构体大小:\n");
    printf("  {char, int, short}:    %lu 字节\n", sizeof(struct Test1));
    printf("  {char, char, int}:     %lu 字节\n", sizeof(struct Test2));
    printf("  {int, char, short}:    %lu 字节\n", sizeof(struct Test3));
    printf("  {short, char, int}:    %lu 字节\n", sizeof(struct Test4));

    /* 打印成员偏移量 */
    printf("\nTest1 成员偏移:\n");
    printf("  a 偏移: %lu\n", (size_t)&((struct Test1 *)0)->a);
    printf("  b 偏移: %lu\n", (size_t)&((struct Test1 *)0)->b);
    printf("  c 偏移: %lu\n", (size_t)&((struct Test1 *)0)->c);

    printf("\n");
}

/* ==================== 3. 位域 ==================== */

void demo_bit_field() {
    printf("===== 3. 位域 =====\n");

    /* 用位域表示文件权限 */
    struct FilePermission {
        unsigned int readable   : 1;  /* 可读 */
        unsigned int writable   : 1;  /* 可写 */
        unsigned int executable : 1;  /* 可执行 */
        unsigned int reserved   : 29; /* 保留 */
    };

    struct FilePermission fp;
    fp.readable = 1;
    fp.writable = 1;
    fp.executable = 0;
    fp.reserved = 0;

    printf("sizeof(FilePermission) = %lu 字节\n", sizeof(struct FilePermission));
    printf("可读: %s\n", fp.readable ? "是" : "否");
    printf("可写: %s\n", fp.writable ? "是" : "否");
    printf("可执行: %s\n", fp.executable ? "是" : "否");

    /* 用位域表示颜色（RGB各8位） */
    struct Color {
        unsigned int red   : 8;
        unsigned int green : 8;
        unsigned int blue  : 8;
        unsigned int alpha : 8;
    };

    struct Color c;
    c.red = 255;
    c.green = 128;
    c.blue = 0;
    c.alpha = 255;

    printf("\n颜色: RGBA(%d, %d, %d, %d)\n", c.red, c.green, c.blue, c.alpha);
    printf("sizeof(Color) = %lu 字节\n", sizeof(struct Color));

    printf("\n");
}

/* ==================== 4. 大小端判断 ==================== */

int is_little_endian() {
    /* 方法1：用 union */
    union {
        int i;
        char c;
    } test;
    test.i = 1;
    return test.c == 1;

    /* 方法2：用指针强转 */
    /* int a = 1; */
    /* return *(char *)&a == 1; */
}

void demo_endian() {
    printf("===== 4. 大小端判断 =====\n");

    if (is_little_endian()) {
        printf("当前系统是：小端模式（Little-Endian）\n");
    } else {
        printf("当前系统是：大端模式（Big-Endian）\n");
    }

    /* 直观展示 */
    int a = 0x12345678;
    unsigned char *bytes = (unsigned char *)&a;
    printf("\nint a = 0x12345678 的字节存储:\n");
    printf("  地址 %p: 0x%02x\n", (void *)(bytes + 0), bytes[0]);
    printf("  地址 %p: 0x%02x\n", (void *)(bytes + 1), bytes[1]);
    printf("  地址 %p: 0x%02x\n", (void *)(bytes + 2), bytes[2]);
    printf("  地址 %p: 0x%02x\n", (void *)(bytes + 3), bytes[3]);

    if (bytes[0] == 0x78) {
        printf("  低位字节 0x78 在低地址 → 小端\n");
    } else {
        printf("  高位字节 0x12 在低地址 → 大端\n");
    }

    printf("\n");
}

/* ==================== 5. union 用途 ==================== */

void demo_union() {
    printf("===== 5. union 用途 =====\n");

    /* 基本用法：所有成员共享内存 */
    union Data {
        int i;
        float f;
        char str[20];
    };

    union Data d;
    printf("sizeof(Data) = %lu 字节\n", sizeof(d));

    d.i = 42;
    printf("d.i = %d\n", d.i);

    d.f = 3.14f;
    printf("d.f = %f\n", d.f);
    printf("d.i = %d (已被覆盖！)\n", d.i);

    /* 用途1：类型转换（查看 float 的 IEEE 754 表示） */
    union FloatConverter {
        float f;
        unsigned int u;
        unsigned char bytes[4];
    };

    union FloatConverter fc;
    fc.f = 3.14f;
    printf("\nfloat 3.14 的 IEEE 754 表示:\n");
    printf("  十六进制: 0x%08x\n", fc.u);
    printf("  字节: %02x %02x %02x %02x\n",
           fc.bytes[0], fc.bytes[1], fc.bytes[2], fc.bytes[3]);

    /* 用途2：变体类型（类似 C++ 的 std::variant） */
    struct Value {
        int type;  /* 0=int, 1=float, 2=string */
        union {
            int int_val;
            float float_val;
            char str_val[20];
        } data;
    };

    struct Value v;
    v.type = 0;
    v.data.int_val = 42;
    printf("\n变体类型: type=%d, int_val=%d\n", v.type, v.data.int_val);

    v.type = 1;
    v.data.float_val = 3.14f;
    printf("变体类型: type=%d, float_val=%f\n", v.type, v.data.float_val);

    printf("\n");
}

/* ==================== 6. 结构体嵌套 ==================== */

void demo_nested_struct() {
    printf("===== 6. 结构体嵌套 =====\n");

    typedef struct {
        int year;
        int month;
        int day;
    } Date;

    typedef struct {
        char name[20];
        Date birthday;
        float score;
    } StudentWithBirthday;

    StudentWithBirthday s = {
        .name = "王五",
        .birthday = {.year = 2000, .month = 5, .day = 15},
        .score = 92.5f
    };

    printf("姓名: %s\n", s.name);
    printf("生日: %d-%02d-%02d\n",
           s.birthday.year, s.birthday.month, s.birthday.day);
    printf("成绩: %.1f\n", s.score);

    /* 通过指针访问嵌套成员 */
    StudentWithBirthday *p = &s;
    printf("通过指针: %s, %d\n", p->name, p->birthday.year);

    printf("\n");
}

/* ==================== main ==================== */

int main() {
    printf("========================================\n");
    printf("  struct 与 union 示例代码\n");
    printf("  作者：青松与桑叶\n");
    printf("========================================\n\n");

    demo_struct_basic();
    demo_struct_alignment();
    demo_bit_field();
    demo_endian();
    demo_union();
    demo_nested_struct();

    printf("========================================\n");
    printf("  所有示例运行完毕！\n");
    printf("========================================\n");

    return 0;
}
