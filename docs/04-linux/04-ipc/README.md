# 4.4 进程间通信

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

## 目录

- [1. IPC 总览与选型](#1-ipc-总览与选型)
- [2. 匿名管道 pipe](#2-匿名管道-pipe)
- [3. 命名管道 FIFO](#3-命名管道-fifo)
- [4. POSIX 消息队列](#4-posix-消息队列)
- [5. 共享内存](#5-共享内存)
- [6. 信号量](#6-信号量)
- [7. Unix Domain Socket](#7-unix-domain-socket)
- [8. 底层原理串讲](#8-底层原理串讲)
- [9. 踩坑清单](#9-踩坑清单)
- [10. 面试真题](#10-面试真题)
- [11. 参考资料](#11-参考资料)
- [12. 上一节 | 下一节](#12-上一节--下一节)

---

## 1. IPC 总览与选型

进程有独立地址空间，默认不能直接访问对方内存。要让它们协作，就需要 **IPC：Inter-Process Communication，进程间通信**。

面试不要只背名字，要先说选型：

| 方式 | 数据形态 | 是否要求亲缘进程 | 典型场景 |
|---|---|---|---|
| 匿名管道 pipe | 字节流 | 通常是父子/兄弟 | shell 管道、简单父子传数据 |
| 命名管道 FIFO | 字节流 | 否 | 本机无亲缘进程简单传数据 |
| POSIX 消息队列 | 有边界消息，可带优先级 | 否 | 任务分发、控制消息 |
| 共享内存 | 共享内存区 | 否 | 大数据、高频低延迟 |
| 信号量 | 同步计数 | 否 | 互斥、同步、生产者消费者 |
| Unix Socket | 字节流/数据报/顺序包 | 否 | 本机服务通信、传 fd、鉴权 |

一句话选型：**小数据流用管道，消息任务用消息队列，大数据用共享内存，同步用信号量，本机服务化通信用 Unix Socket。**

Linux 上还有 System V IPC：`msgget/shmget/semget` 等。它们更老，接口繁琐；新代码通常优先 POSIX IPC 或 Unix Socket。面试可以说知道 System V，但工程上更推荐 POSIX 接口。

---

## 2. 匿名管道 pipe

管道是最经典的 IPC。你可以把它想象成一根水管：一头写，一头读。`pipe(fd)` 会返回两个文件描述符：

- `fd[0]`：读端。
- `fd[1]`：写端。

管道是**半双工**的，数据只能从写端流向读端。它没有消息边界，是字节流；你写两次，读端可能一次读到，也可能分多次读到。

### 2.1 可运行示例：父子进程通过管道传字符串

保存为 `pipe_demo.c`：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int fd[2];
    if (pipe(fd) < 0) {              /* 创建管道：fd[0]读，fd[1]写 */
        perror("pipe");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {                  /* 子进程负责写 */
        close(fd[0]);                /* 关闭不用的读端 */
        const char *msg = "hello pipe";
        if (write(fd[1], msg, strlen(msg)) < 0) {
            perror("write");
            _exit(EXIT_FAILURE);
        }
        close(fd[1]);                /* 写完关闭写端，让读端看到 EOF */
        _exit(EXIT_SUCCESS);
    }

    close(fd[1]);                    /* 父进程负责读，关闭写端 */
    char buf[64] = {0};
    ssize_t n = read(fd[0], buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        return EXIT_FAILURE;
    }
    buf[n] = '\0';
    printf("parent read: %s\n", buf);

    close(fd[0]);
    waitpid(pid, NULL, 0);
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o pipe_demo pipe_demo.c
./pipe_demo
```

### 2.2 为什么必须关闭不用的 fd

`fork` 后父子都持有读端和写端。如果父进程不关闭自己的写端，即使子进程退出并关闭写端，内核仍认为“还有进程可能写”，父进程 `read` 就可能一直阻塞，等不到 EOF。

规则：

- 读端进程关闭写端。
- 写端进程关闭读端。
- 所有写端都关闭后，读端 `read` 返回 0。
- 所有读端都关闭后，写端 `write` 触发 `SIGPIPE`；若忽略该信号，则返回 `EPIPE`。

### 2.3 管道容量与 PIPE_BUF

管道有内核缓冲区，容量有限。Linux 默认通常是 16 页，在 4KiB 页大小下是 64KiB；可用 `fcntl` 的 `F_GETPIPE_SZ` / `F_SETPIPE_SZ` 查询或调整。

`PIPE_BUF` 是原子写边界。Linux 上通常是 4096 字节：

| 写入大小 | 阻塞模式 | 语义 |
|---|---|---|
| `n <= PIPE_BUF` | 阻塞 | 要么完整写入 n 字节，要么等待空间 |
| `n <= PIPE_BUF` | 非阻塞 | 空间够则完整写入，不够返回 EAGAIN |
| `n > PIPE_BUF` | 阻塞 | 可能分多次写，可能与其他写者交错 |
| `n > PIPE_BUF` | 非阻塞 | 可能部分写，也可能交错 |

所以多进程写同一个管道时，不要假设大写入天然原子。需要消息边界，要么控制单条消息不超过 `PIPE_BUF`，要么自己设计长度头，要么改用消息队列/Unix Socket。

---

## 3. 命名管道 FIFO

匿名管道没有名字，通常靠 `fork` 继承 fd。FIFO 有文件系统路径，任何有权限的进程都能打开它，因此可以用于无亲缘进程。

创建：

```bash
mkfifo /tmp/demo_fifo
```

终端 A 读：

```bash
cat /tmp/demo_fifo
```

终端 B 写：

```bash
echo "hello fifo" > /tmp/demo_fifo
```

用完删除：

```bash
rm /tmp/demo_fifo
```

FIFO 和匿名管道一旦打开，读写语义基本一致：字节流、无消息边界、空读阻塞、满写阻塞。区别只在创建和打开方式。

踩坑：FIFO 的 `open` 可能阻塞。只读打开会等写者，只写打开会等读者。若不想阻塞，可以用 `O_NONBLOCK`，但要处理 `EAGAIN`、`ENXIO` 等情况。

---

## 4. POSIX 消息队列

消息队列适合“发一条一条消息”的场景。和管道不同，它保留消息边界，还支持优先级。

核心接口：

| 接口 | 作用 |
|---|---|
| `mq_open` | 创建/打开消息队列，名字形如 `/name` |
| `mq_send` | 发送一条消息，可指定优先级 |
| `mq_receive` | 接收一条消息，优先取高优先级 |
| `mq_close` | 关闭当前进程的描述符 |
| `mq_unlink` | 删除队列名字 |

### 4.1 可运行示例：发送并接收一条消息

保存为 `mq_demo.c`：

```c
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    struct mq_attr attr;
    attr.mq_flags = 0;          /* 阻塞模式 */
    attr.mq_maxmsg = 10;        /* 队列最多容纳 10 条消息 */
    attr.mq_msgsize = 128;      /* 每条消息最大 128 字节 */
    attr.mq_curmsgs = 0;        /* 创建时当前消息数 */

    mqd_t mq = mq_open("/demo_mq", O_CREAT | O_RDWR, 0600, &attr);
    if (mq == (mqd_t)-1) {
        perror("mq_open");
        return EXIT_FAILURE;
    }

    const char *msg = "hello mqueue";
    if (mq_send(mq, msg, strlen(msg), 1) < 0) {  /* 优先级 1 */
        perror("mq_send");
        return EXIT_FAILURE;
    }

    char buf[129];              /* 接收缓冲区必须 >= mq_msgsize */
    unsigned int prio = 0;
    ssize_t n = mq_receive(mq, buf, sizeof(buf), &prio);
    if (n < 0) {
        perror("mq_receive");
        return EXIT_FAILURE;
    }
    buf[n] = '\0';
    printf("recv: %s, prio=%u\n", buf, prio);

    mq_close(mq);
    mq_unlink("/demo_mq");      /* 删除名字，避免内核对象残留 */
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o mq_demo mq_demo.c -lrt
./mq_demo
```

### 4.2 消息队列的关键属性

- 名字以 `/` 开头，后面不能再有 `/`。
- 消息按优先级投递，高优先级先出；同优先级按先进先出。
- Linux 优先级范围可到 32767，但 POSIX 只要求至少支持 0～31。
- 队列有内核持久性：不 `mq_unlink`，它会存在到系统关机。
- Linux 上消息队列描述符本质是 fd，因此可以接入 `select/poll/epoll`，但这不是 POSIX 可移植保证。

踩坑：`mq_receive` 的缓冲区必须不小于队列的 `mq_msgsize`，否则失败。不要拿管道字节流思维去读消息队列；它一次收一条完整消息。

---

## 5. 共享内存

共享内存是最快的 IPC 之一，因为数据不需要在进程间反复拷贝。多个进程把同一块物理内存映射到各自地址空间，然后直接读写。

POSIX 共享内存流程：

1. `shm_open` 创建/打开共享内存对象。
2. `ftruncate` 设置大小。
3. `mmap` 映射到进程地址空间。
4. 读写共享区。
5. `munmap` 解除映射。
6. `shm_unlink` 删除名字。

### 5.1 可运行示例：父子进程共享字符串

保存为 `shm_demo.c`：

```c
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SHM_NAME "/demo_shm"
#define SHM_SIZE 4096

int main(void)
{
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd < 0) {
        perror("shm_open");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, SHM_SIZE) < 0) {   /* 新对象默认长度为 0，必须设置大小 */
        perror("ftruncate");
        return EXIT_FAILURE;
    }

    char *addr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        perror("mmap");
        return EXIT_FAILURE;
    }
    close(fd);                           /* 映射建立后可关闭 fd */

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        strcpy(addr, "hello shared memory");  /* 子进程写共享区 */
        _exit(EXIT_SUCCESS);
    }

    waitpid(pid, NULL, 0);               /* 等子进程写完再读 */
    printf("parent read: %s\n", addr);

    munmap(addr, SHM_SIZE);
    shm_unlink(SHM_NAME);                /* 删除名字 */
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o shm_demo shm_demo.c -lrt
./shm_demo
```

### 5.2 共享内存快，但不自动安全

共享内存只解决“数据怎么共享”，不解决“什么时候能读写”。多个进程同时写同一块区域，仍然会有数据竞争。

常见组合：

- 共享内存 + 信号量：生产者消费者。
- 共享内存 + mutex/cond：需要进程间共享的 pthread 同步对象。
- 共享内存 + 原子操作：简单计数、标志位。

`shm_unlink` 只是删除名字，不会立刻销毁所有映射。只有名字被删除且所有进程都解除映射后，对象才真正释放。

---

## 6. 信号量

信号量是一个非负整数计数器，用来做同步和互斥：

- `sem_wait`：P 操作，计数减 1；如果当前为 0，阻塞等待。
- `sem_post`：V 操作，计数加 1，唤醒等待者。

POSIX 信号量分两种：

| 类型 | 创建方式 | 适用 |
|---|---|---|
| 命名信号量 | `sem_open("/name")` | 无亲缘进程 |
| 无名信号量 | `sem_init` | 线程间，或放在共享内存中的进程间 |

### 6.1 可运行示例：命名信号量做互斥

保存为 `sem_demo.c`：

```c
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#define SEM_NAME "/demo_sem"

int main(void)
{
    /* 初值 1，表示同一时刻只允许一个进程进入临界区 */
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0600, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        sem_wait(sem);                   /* 子进程申请进入临界区 */
        printf("child in critical section\n");
        sleep(1);
        sem_post(sem);                   /* 离开临界区 */
        _exit(EXIT_SUCCESS);
    }

    sem_wait(sem);                       /* 父进程也申请进入临界区 */
    printf("parent in critical section\n");
    sem_post(sem);

    waitpid(pid, NULL, 0);
    sem_close(sem);
    sem_unlink(SEM_NAME);                /* 删除命名信号量 */
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -pthread -o sem_demo sem_demo.c -lrt
./sem_demo
```

### 6.2 信号量和 mutex 的区别

| 对比项 | 信号量 | mutex |
|---|---|---|
| 本质 | 计数器 | 互斥锁 |
| 典型用途 | 同步、资源数量控制 | 保护临界区 |
| 是否强调所有权 | 不强调，谁都可以 post | 通常谁加锁谁解锁 |
| 进程间使用 | 命名信号量或共享内存无名信号量 | 需设置 process-shared 属性 |

面试可以说：互斥用 mutex 更自然；控制“有几个资源可用”或做生产者消费者，用信号量更自然。

---

## 7. Unix Domain Socket

Unix Domain Socket，也叫本机 socket，用于同一台机器上的进程通信。它走 socket API，但不经过网络协议栈，通常比 TCP loopback 更轻。

支持三种类型：

| 类型 | 语义 |
|---|---|
| `SOCK_STREAM` | 字节流，类似 TCP |
| `SOCK_DGRAM` | 数据报，保留消息边界，Linux 上可靠不乱序 |
| `SOCK_SEQPACKET` | 连接式顺序包，保留边界且有序 |

地址有三种：

-  pathname：绑定到文件系统路径，例如 `/tmp/app.sock`。
- unnamed：未绑定路径，`socketpair` 创建的两个 socket 就是无名。
- abstract：Linux 抽象命名空间，`sun_path[0] == '\0'`，不占文件系统路径。

### 7.1 可运行示例：socketpair 父子通信

保存为 `unix_pair.c`：

```c
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    int sv[2];
    /* 创建一对已连接的 Unix socket，全双工 */
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) < 0) {
        perror("socketpair");
        return EXIT_FAILURE;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0) {
        close(sv[0]);                    /* 子进程只用 sv[1] */
        const char *msg = "hello unix socket";
        if (write(sv[1], msg, strlen(msg)) < 0) {
            perror("write");
            _exit(EXIT_FAILURE);
        }
        close(sv[1]);
        _exit(EXIT_SUCCESS);
    }

    close(sv[1]);                        /* 父进程只用 sv[0] */
    char buf[64] = {0};
    ssize_t n = read(sv[0], buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        return EXIT_FAILURE;
    }
    buf[n] = '\0';
    printf("parent read: %s\n", buf);

    close(sv[0]);
    waitpid(pid, NULL, 0);
    return EXIT_SUCCESS;
}
```

编译运行：

```bash
gcc -Wall -std=gnu11 -o unix_pair unix_pair.c
./unix_pair
```

### 7.2 Unix Socket 的两大高级能力

1. **传文件描述符：`SCM_RIGHTS`。** 发送方把 fd 通过辅助数据发给接收方，内核在接收方进程里安装一个指向同一 open file description 的新 fd。接收方拿到的 fd 编号可能不同，但共享文件偏移、状态标志等。
2. **传进程凭据：`SCM_CREDENTIALS` / `SO_PEERCRED`。** 可以拿到对端 PID、UID、GID，用于本机服务鉴权。

pathname socket 踩坑：`bind` 前如果路径已存在，要先 `unlink`，否则 `EADDRINUSE`。服务退出后也要清理 socket 文件。抽象 socket 不占文件，所有引用关闭后自动消失，但它是 Linux 扩展，不可移植。

---

## 8. 底层原理串讲

### 8.1 管道：内核环形缓冲

管道不是磁盘文件，而是内核里的缓冲区。写端把数据拷入内核缓冲，读端再从内核缓冲拷出。它不能 `lseek`，因为不是可随机访问的文件。

### 8.2 消息队列：内核消息链表

POSIX 消息队列由内核维护，按优先级组织消息。Linux 上可在 `/dev/mqueue` 查看队列对象。队列有容量和消息大小限制，受 `/proc/sys/fs/mqueue/*` 和 `RLIMIT_MSGQUEUE` 约束。

### 8.3 共享内存：同一物理页映射到多个地址空间

Linux POSIX 共享内存通常落在 tmpfs，例如 `/dev/shm`。`mmap` 后，多个进程的虚拟地址可以映射到同一批物理页。数据不需要经过内核缓冲区中转，所以快；但同步必须自己做。

### 8.4 信号量：计数 + 等待队列

信号量底层需要原子操作和阻塞唤醒。Linux NPTL 下大量使用 futex：无竞争时用户态原子操作，有竞争时进入内核等待。命名信号量通常也在 `/dev/shm` 下以 `sem.name` 形式存在。

### 8.5 Unix Socket：本机 socket 缓冲

Unix Socket 使用 socket 缓冲区，支持流控、阻塞/非阻塞、epoll。传 fd 时，内核复制的是对 open file description 的引用，而不是简单传一个整数。

---

## 9. 踩坑清单

1. **管道不关无用 fd，EOF 等不到。** 父子都要关闭自己不用的一端。
2. **管道大写入不原子。** 多写者场景超过 `PIPE_BUF` 可能交错。
3. **FIFO open 阻塞。** 只有读端或只有写端时可能卡住。
4. **消息队列缓冲区太小。** `mq_receive` 缓冲区必须 >= `mq_msgsize`。
5. **共享内存不同步。** 快不等于安全，必须配合同步机制。
6. **命名 IPC 对象残留。** mq/shm/sem 不 unlink，会内核持久存在。
7. **Unix socket 路径残留。** bind 前 unlink 旧路径，退出后清理。
8. **传 fd 不是传 fd 数字。** 接收方 fd 编号可能不同，但指向同一打开文件描述。
9. **System V 和 POSIX 混背。** 面试要分清 `msgget` 和 `mq_open`、`shmget` 和 `shm_open`。
10. **把 IPC 当线程同步。** 进程间共享内存里的 pthread mutex 必须设置 process-shared，否则默认只适合线程间。

---

## 10. 面试真题

### Q1：Linux 常见 IPC 有哪些，怎么选？

**三句话答案：** 常见有管道、FIFO、消息队列、共享内存、信号量、Unix Socket，以及老的 System V IPC。小数据流用管道，有边界消息用消息队列，大数据低延迟用共享内存，同步用信号量，本机服务通信用 Unix Socket。选型要看数据形态、是否亲缘、性能、同步需求和工程复杂度。

### Q2：匿名管道和 FIFO 有什么区别？

**三句话答案：** 匿名管道用 pipe 创建，没有文件名，通常靠 fork 继承 fd，适合亲缘进程。FIFO 用 mkfifo 创建，有文件系统路径，无亲缘进程也能打开。两者打开后的读写语义基本一致，都是字节流、无消息边界。

### Q3：为什么管道通信要关闭不用的文件描述符？

**三句话答案：** fork 后父子都持有读写两端，如果不关闭无用端，内核会认为仍有潜在写者或读者。读端可能等不到 EOF，写端也可能无法及时收到 SIGPIPE/EPIPE。关闭无用端能让管道生命周期和错误通知符合预期。

### Q4：PIPE_BUF 有什么意义？

**三句话答案：** PIPE_BUF 是管道原子写的边界，Linux 上通常是 4096 字节。小于等于 PIPE_BUF 的写在阻塞模式下会完整写入，不会与其他写者交错。大于 PIPE_BUF 的写可能分多次完成，也可能交错，所以多写者要自己保证消息边界。

### Q5：共享内存为什么快，为什么还需要同步？

**三句话答案：** 共享内存把同一物理页映射到多个进程，读写不需要内核中转拷贝，所以快。但它只共享数据，不提供互斥和顺序保证。多进程同时读写仍会竞争，必须配合信号量、锁或原子操作。

### Q6：POSIX 消息队列和管道有什么区别？

**三句话答案：** 管道是字节流，没有消息边界；消息队列一次发送和接收一条完整消息。消息队列支持优先级，高优先级消息先被接收。管道更轻更通用，消息队列更适合任务分发和控制消息。

### Q7：信号量和互斥锁有什么区别？

**三句话答案：** 信号量是计数器，适合表示可用资源数量或做同步。互斥锁强调临界区所有权，通常谁加锁谁解锁。进程间可以用命名信号量，或把无名信号量放在共享内存里；mutex 做进程间同步则需要 process-shared 属性。

### Q8：Unix Socket 比 TCP loopback 好在哪？

**三句话答案：** Unix Socket 用于本机进程通信，不经过完整网络协议栈，通常更轻。它支持 pathname、abstract 地址，还能传 fd 和进程凭据。需要本机服务化、鉴权、传描述符时，Unix Socket 比 TCP loopback 更合适。

### Q9：Unix Socket 怎么传文件描述符？

**三句话答案：** 通过 sendmsg/recvmsg 的辅助数据 SCM_RIGHTS 传递 fd。内核在接收方进程安装一个新 fd，指向同一个 open file description。接收方 fd 编号可能不同，但共享文件偏移和打开状态，不是简单传整数。

### Q10：命名 IPC 对象为什么要 unlink？

**三句话答案：** POSIX 消息队列、共享内存、命名信号量都有内核持久性。进程退出不代表对象自动删除，不 unlink 会残留到系统关机。工程上要在合适时机 unlink，并理解 unlink 只删名字，已打开/映射的引用仍可使用。

---

## 11. 参考资料

以下资料用于核对管道、消息队列、共享内存、信号量和 Unix Socket 的接口语义；正文讲解、示例组织与面试答案均为原创重写。

1. pipe(7) — Linux manual page：https://man7.org/linux/man-pages/man7/pipe.7.html
2. mq_overview(7) — Linux manual page：https://man7.org/linux/man-pages/man7/mq_overview.7.html
3. shm_overview(7) — Linux manual page：https://man7.org/linux/man-pages/man7/shm_overview.7.html
4. sem_overview(7) — Linux manual page：https://man7.org/linux/man-pages/man7/sem_overview.7.html
5. unix(7) — Linux manual page：https://man7.org/linux/man-pages/man7/unix.7.html

---

## 12. 上一节 | 下一节

- 上一节：[4.3 进程与线程（Linux 视角）](../03-process-thread/README.md)
- 下一节：4.5 Shell 与自动化（待写）

> 作者：青松与桑叶。IPC 的本质不是背 API，而是理解数据在哪里、谁负责同步、生命周期谁清理。
