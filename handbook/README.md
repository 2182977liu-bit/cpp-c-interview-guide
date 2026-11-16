# C++ 高频面试题全解手册（7 大模块 · 55 题）

> 作者：青松与桑叶 | 配套教程：[C/C++ 面试八股文完全指南](../README.md)
>
> 本手册与教程章节互为补充：教程讲原理与推导，手册给"面场上怎么说"的答题脚本。同一知识点两处都能看到时，以教程的展开为准。

按面试考察维度分为 7 个模块，每道题统一结构：

- **场景还原**——面试官怎么问、坑在哪
- **三句话版本**——可直接背的答案骨架
- **展开讲**——原理、底层机制、内存模型、表格对照
- **高频追问**——面试官连环追问与标准应对

## 模块清单

- [01_基础与语法.md](01-basics/README.md)（13 题）
- [02_面向对象.md](02-oop/README.md)（11 题）
- [03_内存管理.md](03-memory/README.md)（7 题）
- [04_智能指针.md](04-smart-pointer/README.md)（3 题）
- [05_Cpp11与现代C++.md](05-modern-cpp/README.md)（11 题）
- [06_STL与容器.md](06-stl/README.md)（8 题）
- [07_并发与IO.md](07-concurrency-io/README.md)（2 题）

## 覆盖主题速览

| 模块 | 覆盖要点 |
| --- | --- |
| 基础与语法 | 三大特性、指针/引用、struct/class/union、static/const、extern C、volatile、inline 与宏、auto/decltype、sizeof/strlen、浮点比较、变量作用域、四种 cast |
| 面向对象 | 构造函数、虚构造/虚析构、重载/重写、多态、虚函数与纯虚函数、虚表实现、多重继承与菱形继承、final、深浅拷贝、this 指针、单例模式 |
| 内存管理 | 堆与栈、new/malloc、free/delete、placement new、内存泄漏/野指针/越界、碎片与溢出、避免碎片 |
| 智能指针 | 三种智能指针选型、控制块与引用计数、线程安全性 |
| C++11 与现代 C++ | 新特性全景、多线程库、左右值引用、移动语义、完美转发、move/forward、Lambda、仿函数性能、RAII、异常处理、协程 |
| STL 与容器 | 容器分类选型、allocator、迭代器失效、map/unordered_map、底层实现、rehash、vector 扩容、push_back/emplace_back |
| 并发与 I/O | 互斥锁与自旋锁、select/poll/epoll |
