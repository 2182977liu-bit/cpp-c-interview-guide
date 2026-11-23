# 4.3 进程与线程（Linux 视角）

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

## 目录

- [1. 进程与线程的基本概念](#1-进程与线程的基本概念)
- [2. fork：一次调用两次返回](#2-fork一次调用两次返回)
- [3. exec：换程序不换进程](#3-exec换程序不换进程)
- [4. wait：回收子进程](#4-wait回收子进程)
- [5. 僵尸进程与孤儿进程](#5-僵尸进程与孤儿进程)
- [6. Linux 线程模型](#6-linux-线程模型)
- [7. pthread 的共享与私有](#7-pthread-的共享与私有)
- [8. CPU 亲和性](#8-cpu-亲和性)
- [9. 多线程程序里 fork 的坑](#9-多线程程序里-fork-的坑)
- [10. 面试真题](#10-面试真题)
- [11. 参考资料](#11-参考资料)
- [12. 上一节 | 下一节](#12-上一节--下一节)

---

## 1. 进程与线程的基本概念

先用一句话区分：**程序是磁盘上的文件，进程是运行中的程序，线程是进程里的一条执行流。**

你编译出来的 `a.out` 只是一个 ELF 文件。当你执行 `./a.out`，内核会创建进程，把代码、数据、堆、栈装进地址空间，再交给调度器运行。一个进程里可以只有一条执行流，也可以有多条执行流；多条执行流就是多线程。

| 概念 | 是什么 | 是否拥有独立地址空间 | 典型开销 |
|---|---|---|---|
| 程序 | 磁盘上的可执行文件 | 不涉及运行 | 无 |
| 进程 | 资源分配的基本单位 | 是 | 页表、文件表、信号处理等 |
| 线程 | CPU 调度的基本单位 | 同进程内共享 | 栈、寄存器上下文、调度实体 |
| 任务 task | Linux 内核对进程/线程的统一抽象 | 取决于 clone 标志 | 一个 `task_struct` |

Linux 有个很关键的视角：**内核并不把“进程”和“线程”看成两种完全不同的东西。** 它们都是 `task_struct`，区别在于共享了多少资源。普通进程之间地址空间、文件表、信号处理大多独立；线程之间共享地址空间、文件表、信号处理，但各有自己的栈和调度身份。

面试时不要只背“进程重、线程轻”。更准确的说法是：进程隔离强，线程共享多；共享多带来通信方便，也带来数据竞争和同步成本。

---

## 2. fork：一次调用两次返回

`fork()` 用来创建子进程。它最反直觉的地方是：**调用一次，返回两次。**

- 在父进程里，返回子进程的 PID，正数。
- 在子进程里，返回 0。
- 失败时，只在父进程里返回 -1，并设置 `errno`，不会创建子进程。

为什么能返回两次？因为 `fork` 之后，父子进程都从 `fork` 返回处继续执行。它们代码相同，但 PID 不同，返回值不同，所以可以用 `if` 分流。

### 2.1 可运行示例：fork + wait

保存为 `fork_wait.c`：

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();              /* 创建子进程 */
    if (pid < 0) {                   /* 只有父进程会走到失败分支 */
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {                  /* 子进程：fork 返回 0 */
        printf("child: pid=%d ppid=%d\n",
               (int)getpid(), (int)getppid());
        _exit(42);                   /* 子进程退出，退出码 42 */
    }

    /* 父进程：fork 返回子进程 PID */
    printf("parent: pid=%d child=%d\n",
           (int)getpid(), (int)pid);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {  /* 等待指定子进程结束 */
        perror("waitpid");
        return EXIT_FAILURE;
    }

    if (WIFEXITED(status)) {         /* 子进程是否正常退出 */
        printf("child exit code = %d\n", WEXITSTATUS(status));
    }
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o fork_wait fork_wait.c
./fork_wait
```

你会看到父进程打印子进程 PID，子进程打印自己的 PID 和父进程 PID，最后父进程拿到退出码 42。

### 2.2 fork 到底复制了什么

`fork` 不是把父进程内存完整拷贝一份。Linux 使用 **写时复制 Copy-On-Write**：

1. 子进程刚创建时，父子页表指向同一批物理页。
2. 这些页被标记为只读。
3. 谁先写某一页，谁触发缺页异常。
4. 内核再为写者复制那一页，让它拥有独立副本。

所以 `fork` 的主要成本不是复制全部内存，而是复制页表、创建子进程 `task_struct`、设置调度关系。父进程内存越大，页表越多，`fork` 越慢。

| 资源 | fork 后父子关系 |
|---|---|
| 地址空间 | 逻辑独立，物理页写时复制 |
| 文件描述符 | 子进程继承副本，但指向同一个 open file description |
| 文件偏移 | 共享，父子读写同一个 fd 会互相影响偏移 |
| PID | 子进程有新 PID |
| 待处理信号 | 子进程初始为空 |
| 定时器、内存锁、记录锁 | 多数不继承，具体看手册 |
| 线程 | 子进程只有调用 fork 的那一条线程 |

最后一行很重要：**多线程程序里 fork，子进程不会带上其他线程。** 但地址空间里可能还留着其他线程持有的锁状态，这就是后面第 9 节的坑。

### 2.3 fork / vfork / clone 对比

| 接口 | 地址空间 | 父进程是否阻塞 | 典型用途 |
|---|---|---|---|
| `fork` | 写时复制，逻辑独立 | 不阻塞 | 创建普通子进程 |
| `vfork` | 历史上共享地址空间 | 父进程阻塞到子进程 exec 或退出 | 早期优化，现在少用 |
| `clone` | 由 flags 决定共享哪些资源 | 可配置 | 实现线程、容器、命名空间 |

`glibc` 的 `fork` 包装器底层通常也是走 `clone`，只是标志等价于传统 fork。你不必手动写 `clone` 来创建普通子进程，但理解 `clone` 能帮你理解 Linux 线程模型。

---

## 3. exec：换程序不换进程

`exec` 系列函数不是创建新进程，而是**把当前进程正在运行的程序替换掉**。

可以把它想象成：进程这个“壳”还在，PID 还在，但里面的代码、数据、堆、栈都换成新程序。`execve` 成功后不会返回；如果返回了，说明失败。

### 3.1 可运行示例：fork + exec

保存为 `exec_echo.c`：

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        /* 子进程把自己替换成 /bin/echo */
        execl("/bin/echo", "echo", "hello exec", (char *)NULL);

        /* 只有 execl 失败才会执行到这里 */
        perror("execl");
        _exit(127);
    }

    /* 父进程等待子进程结束 */
    waitpid(pid, NULL, 0);
    printf("parent still alive, pid=%d\n", (int)getpid());
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o exec_echo exec_echo.c
./exec_echo
```

输出里会先出现 `hello exec`，然后父进程继续打印。子进程执行 `/bin/echo` 时，并没有变成另一个 PID；它只是换了一个程序镜像。

### 3.2 execve 的参数

```c
int execve(const char *path, char *const argv[], char *const envp[]);
```

- `path`：要执行的文件路径。
- `argv`：参数数组，必须以 `NULL` 结尾；习惯上 `argv[0]` 是程序名。
- `envp`：环境变量数组，形如 `KEY=value`，也必须以 `NULL` 结尾。

常见变体：

| 函数 | 是否搜索 PATH | 参数形式 | 环境 |
|---|---|---|---|
| `execl` | 否 | 列表 | 继承当前环境 |
| `execlp` | 是 | 列表 | 继承当前环境 |
| `execle` | 否 | 列表 | 显式传入 envp |
| `execv` | 否 | 数组 | 继承当前环境 |
| `execvp` | 是 | 数组 | 继承当前环境 |
| `execve` | 否 | 数组 | 显式传入 envp |

记忆方法：`l` 是 list，`v` 是 vector，`p` 是 search PATH，`e` 是 environment。

### 3.3 exec 的踩坑

1. **exec 成功不返回。** 所以 `exec` 后面紧跟 `perror` 和 `_exit` 是标准写法。
2. **PID 不变。** 不要把 exec 说成“创建新进程”。
3. **文件描述符默认保留。** 如果不想让新程序继承某个 fd，要设置 `FD_CLOEXEC`，或打开时使用 `O_CLOEXEC`。
4. **脚本靠 shebang。** `#! /path/interpreter` 会让内核调用解释器，而不是直接执行文本。
5. **多线程里 exec 会终止其他线程。** 成功后整个进程只剩新程序的一条主执行流。

---

## 4. wait：回收子进程

子进程退出后，内核不会立刻把它的所有信息抹掉。它还要保留 PID、退出状态、资源使用统计，等父进程来取。父进程通过 `wait` / `waitpid` 取走这些信息，子进程才算真正被回收。

### 4.1 wait 与 waitpid

```c
pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);
```

`wait(&status)` 等价于 `waitpid(-1, &status, 0)`，也就是等待任意一个子进程。

`waitpid` 的 `pid` 参数：

| pid 值 | 含义 |
|---|---|
| `> 0` | 等待指定 PID 的子进程 |
| `-1` | 等待任意子进程 |
| `0` | 等待同进程组的任意子进程 |
| `< -1` | 等待进程组 ID 等于 `|pid|` 的任意子进程 |

常用 options：

| 标志 | 作用 |
|---|---|
| `WNOHANG` | 没有子进程退出时立即返回 0，不阻塞 |
| `WUNTRACED` | 也报告被信号停止的子进程 |
| `WCONTINUED` | 也报告被 SIGCONT 恢复的子进程 |

### 4.2 退出状态不要直接当 exit code

`wait` 写回的 `status` 不是子进程 `exit(3)` 里的 3。它是一个编码后的状态，要用宏解析：

```c
int status = 0;
waitpid(pid, &status, 0);

if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);   /* 正常退出码，低 8 位 */
    printf("exit code = %d\n", code);
} else if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);       /* 被哪个信号杀死 */
    printf("killed by signal %d\n", sig);
}
```

面试高频追问：为什么退出码只有 8 位？因为 `WEXITSTATUS` 取的是低 8 位，范围 0～255。你 `exit(256)` 和 `exit(0)` 在父进程看来可能一样。

### 4.3 非阻塞回收与 SIGCHLD

服务端常见写法是循环 `waitpid(-1, &status, WNOHANG)`，一次回收多个已退出子进程：

```c
int status;
pid_t pid;
while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    /* 处理每个退出的子进程 */
}
```

也可以显式把 `SIGCHLD` 设置为 `SIG_IGN`，让内核自动回收子进程。但注意：这和“默认忽略 SIGCHLD”不是一回事。显式 `SIG_IGN` 后，子进程不会变僵尸，但后续 `wait` 可能失败并返回 `ECHILD`。

---

## 5. 僵尸进程与孤儿进程

这两个概念面试几乎必问，但很多人背反。

| 类型 | 谁先退出 | 状态 | 危害 | 处理方式 |
|---|---|---|---|---|
| 僵尸进程 | 子进程先退出，父进程没 wait | 子进程已死，但保留退出信息 | 占 PID/进程表槽位 | 父进程 wait；或父进程退出后被收养回收 |
| 孤儿进程 | 父进程先退出，子进程还活着 | 子进程被 init 或 subreaper 收养 | 通常不直接危害 | 收养者负责后续回收 |

### 5.1 僵尸进程示例

```c
pid_t pid = fork();
if (pid == 0) {
    _exit(0);          /* 子进程立刻退出 */
}
sleep(10);             /* 父进程不 wait，这 10 秒内子进程是僵尸 */
waitpid(pid, NULL, 0); /* 10 秒后回收 */
```

观察命令：

```bash
ps -o pid,ppid,stat,cmd -p <子进程PID>
```

如果 `STAT` 里出现 `Z`，就是 zombie。僵尸进程已经不运行，不占 CPU，也不占用户态内存，但它占着内核里的进程表位置。大量僵尸会导致无法创建新进程。

### 5.2 孤儿进程示例

```c
pid_t pid = fork();
if (pid == 0) {
    sleep(5);
    printf("child ppid = %d\n", (int)getppid());
    _exit(0);
}
exit(0);               /* 父进程先退出，子进程成为孤儿 */
```

子进程会被 PID 1 的 init，或最近的 `PR_SET_CHILD_SUBREAPER` 进程收养。容器里要特别注意：如果 PID 1 不回收子进程，僵尸会堆积。常见做法是使用 tini/dumb-init，或让主进程承担 subreaper 职责。

### 5.3 子进程退出用 exit 还是 _exit

在 `fork` 后的子进程里，尤其是要马上 `exec` 或退出时，优先用 `_exit`。

原因：`exit` 会刷新 stdio 缓冲区、执行 `atexit` 处理函数。父进程缓冲区里可能已有未输出内容，子进程如果走 `exit`，可能把同一份缓冲再刷一遍，造成重复输出。`_exit` 直接进入内核终止，不做用户态清理。

---

## 6. Linux 线程模型

Linux 上现代 POSIX 线程实现是 **NPTL**，它是 1:1 模型：**每个用户线程对应一个内核调度实体。**

这和某些语言运行时的 M:N 协程模型不同。你调用 `pthread_create`，不是只在用户态创建一个轻量对象，而是通过 `clone` 创建一个共享大量资源的内核任务。

### 6.1 clone 标志决定共享什么

可以把 `clone` 理解成“可配置版 fork”：

| 标志 | 共享内容 |
|---|---|
| `CLONE_VM` | 地址空间 |
| `CLONE_FS` | 文件系统信息，如当前目录 |
| `CLONE_FILES` | 文件描述符表 |
| `CLONE_SIGHAND` | 信号处理函数 |
| `CLONE_THREAD` | 放入同一线程组 |

普通 fork 大体等价于只带 `SIGCHLD` 的 clone；pthread 创建线程则会带上共享地址空间、文件表、信号处理等标志。

### 6.2 PID、TID、pthread_t

| 标识 | 获取方式 | 含义 |
|---|---|---|
| PID / TGID | `getpid()` | 进程 ID，也是线程组 ID |
| 内核 TID | `gettid()` | 每条内核任务的唯一 ID |
| pthread_t | `pthread_self()` | POSIX 线程库使用的线程标识 |

同一进程里的所有线程，`getpid()` 返回值相同；但每条线程的 `gettid()` 不同。`ps -T` 或 `top -H` 能看到线程级任务。

面试可以这么说：Linux 内核调度的是 task；用户态看到的进程，是一组共享资源的 task；线程组里的主线程 TID 通常等于进程 PID。

### 6.3 futex：用户态快路径 + 内核慢路径

NPTL 的互斥锁、线程等待等同步原语底层大量使用 `futex`。它的思想是：

1. 没有竞争时，在用户态用原子操作完成加锁/解锁，不进内核。
2. 有竞争、需要阻塞或唤醒时，再陷入内核等待队列。

所以“线程锁一定很慢”不准确。无竞争 mutex 可以很快；真正贵的是竞争、阻塞、唤醒和上下文切换。

---

## 7. pthread 的共享与私有

线程方便，是因为共享；线程危险，也是因为共享。

### 7.1 共享与私有列表

| 资源 | 同进程线程之间 |
|---|---|
| 代码段、全局变量、堆 | 共享 |
| 文件描述符表 | 共享 |
| 当前工作目录、umask | 共享 |
| 信号处理函数 | 共享 |
| 进程 ID、进程组、会话 | 共享 |
| 每条线程的栈 | 私有 |
| 寄存器上下文 | 私有 |
| `errno` | 每线程私有 |
| 信号屏蔽字 | 每线程私有 |
| CPU 亲和性 | 每线程私有 |
| 调度策略/优先级 | 每线程可独立 |

注意：`errno` 是线程私有的，所以多线程里 perror 不会互相覆盖 errno。但全局变量、堆对象、共享容器必须同步。

### 7.2 可运行示例：共享计数器

保存为 `pthread_share.c`：

```c
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static long counter = 0;                            /* 全局变量，线程共享 */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static void *worker(void *arg)
{
    long id = (long)(intptr_t)arg;                  /* 线程编号 */
    for (int i = 0; i < 100000; ++i) {
        pthread_mutex_lock(&lock);                  /* 保护共享计数器 */
        ++counter;
        pthread_mutex_unlock(&lock);
    }
    printf("thread %ld done\n", id);
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;
    if (pthread_create(&t1, NULL, worker, (void *)(intptr_t)1) != 0) {
        perror("pthread_create");
        return EXIT_FAILURE;
    }
    if (pthread_create(&t2, NULL, worker, (void *)(intptr_t)2) != 0) {
        perror("pthread_create");
        return EXIT_FAILURE;
    }

    pthread_join(t1, NULL);                         /* 等待线程结束并回收资源 */
    pthread_join(t2, NULL);

    printf("counter = %ld\n", counter);             /* 应为 200000 */
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -pthread -o pthread_share pthread_share.c
./pthread_share
```

如果去掉 mutex，两个线程同时 `++counter` 就可能丢失更新。`++` 看起来是一行，底层通常是读、改、写三步，不是原子操作。

### 7.3 join 与 detach

| 方式 | 含义 | 适用 |
|---|---|---|
| `pthread_join` | 等待线程结束，回收资源，可拿返回值 | 需要知道线程结果 |
| `pthread_detach` | 线程结束后自动释放资源 | 后台任务，不关心结果 |

一个线程不能既 join 又 detach。线程结束后，`pthread_t` 可能被复用；不要拿已经结束的线程 ID 继续操作。

---

## 8. CPU 亲和性

CPU 亲和性就是规定：**某条线程只能在哪些 CPU 上运行。**

好处：

- 减少线程在不同 CPU 之间迁移带来的缓存失效。
- 把关键线程绑到指定核，降低抖动。
- 配合 NUMA、隔离核、实时任务做性能优化。

代价：

- 绑得太死，可能让某个核过载，其他核空闲。
- 容器/cpuset 限制下，你设置的掩码可能被进一步裁剪。
- 亲和性是每线程属性，不是整个进程一刀切。

### 8.1 可运行示例：绑定到当前可用 CPU

保存为 `affinity.c`：

```c
#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    cpu_set_t cur;
    CPU_ZERO(&cur);

    /* 先读取当前线程允许的 CPU，避免绑到不可用核 */
    if (sched_getaffinity(0, sizeof(cur), &cur) < 0) {
        perror("sched_getaffinity");
        return EXIT_FAILURE;
    }

    int target = -1;
    for (int i = 0; i < CPU_SETSIZE; ++i) {
        if (CPU_ISSET(i, &cur)) {
            target = i;                 /* 选第一个可用 CPU */
            break;
        }
    }
    if (target < 0) {
        fprintf(stderr, "no available CPU\n");
        return EXIT_FAILURE;
    }

    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(target, &set);

    /* pid=0 表示设置当前线程 */
    if (sched_setaffinity(0, sizeof(set), &set) < 0) {
        perror("sched_setaffinity");
        return EXIT_FAILURE;
    }

    printf("bind to CPU %d\n", target);

    /* 做一点计算，方便用 top/ps 观察 */
    volatile long x = 0;
    for (long i = 0; i < 100000000L; ++i) {
        x += i;
    }
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o affinity affinity.c
./affinity
```

命令行也可以：

```bash
taskset -c 0 ./your_program      # 启动时绑到 CPU0
taskset -pc 0 <pid>              # 查看/修改进程主线程亲和性
```

### 8.2 继承关系

- `fork` 创建的子进程继承父进程 CPU 亲和性掩码。
- `execve` 后亲和性掩码保留。
- 线程可以各自设置不同亲和性。
- 实际可运行 CPU 是“你设置的掩码 ∩ 系统在线 CPU ∩ cpuset 允许范围”。

如果 `sched_setaffinity` 返回 `EINVAL`，先检查掩码是否为空、CPU 是否在线、容器 cpuset 是否允许。

---

## 9. 多线程程序里 fork 的坑

这是高级面试常考点：**多线程程序可以 fork，但 fork 后子进程非常危险。**

原因：

1. 子进程只复制调用 fork 的那条线程。
2. 其他线程不会被复制过来。
3. 但地址空间里的锁状态可能被复制过来。
4. 如果某个 mutex 在 fork 瞬间被另一条线程持有，子进程里这个锁可能永远没人释放。

所以 POSIX 要求：多线程程序 fork 后，子进程在 exec 之前只能调用 **async-signal-safe** 函数。像 `malloc`、`printf`、大多数 pthread 函数都不安全。

推荐模式：

```c
pid_t pid = fork();
if (pid == 0) {
    /* 子进程尽快 exec，不要做复杂逻辑 */
    execl("/path/to/program", "program", (char *)NULL);
    _exit(127);
}
```

如果确实需要在 fork 前后处理锁，可以了解 `pthread_atfork` 注册 prepare/parent/child 处理函数。但它不是万能药，复杂锁层级仍然容易出问题。工程上更稳的是：避免在多线程核心路径里 fork；需要启动子进程时，用专门的单线程进程或 posix_spawn 等方案。

---

## 10. 面试真题

### Q1：fork 的返回值为什么有两次？

**三句话答案：** `fork` 会创建子进程，父子进程都从 `fork` 返回处继续执行。父进程返回子进程 PID，子进程返回 0，失败时父进程返回 -1。这样程序可以用返回值区分父子分支，并让父进程拿到子进程 PID 去 wait。

### Q2：fork 为什么没有想象中那么慢？

**三句话答案：** Linux 的 fork 使用写时复制，不会立刻复制全部物理内存。父子页表先指向同一批物理页，只有发生写入时才复制对应页。主要成本是复制页表和创建子进程内核结构，所以父进程地址空间越大，fork 越贵。

### Q3：exec 会创建新进程吗？

**三句话答案：** 不会，exec 是替换当前进程的程序镜像。成功后 PID 不变，但代码、数据、堆、栈被新程序覆盖，并且不再返回。只有失败时才返回 -1，所以 exec 后面通常跟 perror 和 _exit。

### Q4：僵尸进程和孤儿进程有什么区别？

**三句话答案：** 僵尸是子进程已退出但父进程还没 wait，内核保留退出信息供父进程读取。孤儿是父进程先退出，子进程还活着，会被 init 或 subreaper 收养。僵尸占进程表槽位，需要回收；孤儿本身通常不是问题，关键是收养者要负责后续 wait。

### Q5：怎么避免僵尸进程？

**三句话答案：** 父进程应该用 wait/waitpid 回收子进程退出状态。服务端可以用 SIGCHLD 处理函数循环 waitpid(-1, ..., WNOHANG)，或显式 SIG_IGN 让内核自动回收。容器里还要确保 PID 1 或 subreaper 能回收孤儿和僵尸。

### Q6：wait 和 waitpid 有什么区别？

**三句话答案：** wait 阻塞等待任意一个子进程退出，等价于 waitpid(-1, ..., 0)。waitpid 可以指定子进程、进程组，还能用 WNOHANG 非阻塞返回。需要精确控制回收对象或避免阻塞时，用 waitpid 更合适。

### Q7：Linux 下进程和线程的本质区别是什么？

**三句话答案：** Linux 内核统一用 task_struct 描述调度实体，进程和线程都是 task。普通进程之间地址空间等资源隔离，线程则通过 clone 共享地址空间、文件表、信号处理等。线程仍有自己的栈、寄存器上下文、errno、信号屏蔽字和 CPU 亲和性。

### Q8：pthread_t、getpid、gettid 分别是什么？

**三句话答案：** pthread_t 是 POSIX 线程库的线程标识，只在进程内有意义。getpid 返回进程 ID，也就是线程组 ID，同进程线程相同。gettid 返回内核任务 ID，每条线程不同，ps -T 看到的线程级 ID 就是它。

### Q9：多线程程序里 fork 有什么风险？

**三句话答案：** fork 后子进程只有调用 fork 的那条线程，但地址空间里的锁状态可能被复制。如果其他线程正持有锁，子进程可能死锁。因此子进程在 exec 前只能调用 async-signal-safe 函数，工程上应尽快 exec 或避免多线程 fork。

### Q10：CPU 亲和性有什么用，有什么限制？

**三句话答案：** CPU 亲和性限制线程只能在指定 CPU 上运行，可减少迁移和缓存失效，适合低延迟或 NUMA 优化。它是每线程属性，fork 继承、exec 保留。实际可用 CPU 还受在线 CPU 和 cpuset 限制，绑核不当会造成负载不均。

---

## 11. 参考资料

以下资料用于核对系统调用语义、线程模型和亲和性规则；正文讲解、示例组织与面试答案均为原创重写。

1. fork(2) — Linux manual page：https://man7.org/linux/man-pages/man2/fork.2.html
2. execve(2) — Linux manual page：https://man7.org/linux/man-pages/man2/execve.2.html
3. wait(2) — Linux manual page：https://man7.org/linux/man-pages/man2/wait.2.html
4. pthreads(7) — Linux manual page：https://man7.org/linux/man-pages/man7/pthreads.7.html
5. sched_setaffinity(2) — Linux manual page：https://man7.org/linux/man-pages/man2/sched_setaffinity.2.html

---

## 12. 上一节 | 下一节

- 上一节：[4.2 文件 IO 与系统调用](../02-file-io/README.md)
- 下一节：4.4 进程间通信（待写）

> 作者：青松与桑叶。把进程和线程看成“共享多少资源的任务”，很多 Linux 面试题就不再是死记硬背。
