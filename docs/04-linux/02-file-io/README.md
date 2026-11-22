# 4.2 文件 IO 与系统调用

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

---

## 目录

- 一、系统调用与文件描述符：先看清三层结构
- 二、open、read、write：正确写法比函数名更重要
- 三、文件偏移量与 pread/pwrite
- 四、缓冲区：write 返回不等于数据已经落盘
- 五、fsync、fdatasync 与 O_SYNC：持久化到底怎么做
- 六、mmap：把文件映射进虚拟内存
- 七、零拷贝：sendfile 为什么能减少开销
- 八、工程选型：没有一种 IO 永远最快
- 面试真题
- 踩坑提醒

---

## 一、系统调用与文件描述符：先看清三层结构

应用程序运行在用户态，文件系统、磁盘驱动和页缓存由内核管理。`open/read/write/mmap` 是用户程序请求内核完成 IO 的入口，glibc 通常只负责很薄的封装和错误返回处理。

一个文件描述符（file descriptor，简称 fd）不是文件本身，它只是**当前进程 fd 表中的一个小整数下标**。真正要分清的是下面三层：

```text
进程 fd 表                 内核打开文件描述              文件系统
fd 3  ───────────────→  open file description  ─────→  inode
                         当前文件偏移量                   权限、大小、数据块
                         文件状态标志
                         指向文件的引用
```

| 层次 | 保存什么 | 生命周期与共享特点 |
|---|---|---|
| fd | 进程内使用的整数句柄 | `close` 后该编号可再次分配；成功 `open` 通常返回当前最小可用编号 |
| open file description | 文件偏移量、`O_APPEND` 等状态标志 | `dup` 出的 fd、`fork` 继承的 fd 可指向同一份描述，因此共享偏移量 |
| inode | 文件元数据与数据块位置 | 多次独立 `open` 同一路径可得到不同打开文件描述，但最终指向同一 inode |

这三个层次能解释一个高频现象：两个进程继承了同一个 fd，一个进程 `read` 后，另一个进程看到的文件偏移量也变了；如果它们各自独立 `open`，默认则各有各的偏移量。

fd 不是只能代表普通文件，还可以代表管道、socket、终端、事件通知对象等。因此 `read/write` 的具体行为既受通用规则约束，也受 fd 背后对象类型影响。

---

## 二、open、read、write：正确写法比函数名更重要

### 2.1 open 的常用标志

`open` 的访问模式必须三选一：`O_RDONLY`、`O_WRONLY` 或 `O_RDWR`。其他标志通过按位或组合，但访问模式不是“把 `O_RDONLY | O_WRONLY` 拼成读写”。

| 标志 | 通俗含义 | 常见坑 |
|---|---|---|
| `O_CREAT` | 文件不存在就创建 | 必须提供 `mode`，最终权限还受 umask 影响 |
| `O_EXCL` | 与 `O_CREAT` 联用，已存在则失败 | 常用于排他创建，不能单独理解为“只读独占” |
| `O_TRUNC` | 以可写方式打开已有普通文件时截为 0 字节 | 数据立即丢失，配置写入要慎重 |
| `O_APPEND` | 每次写之前把偏移移动到末尾 | “移动到末尾 + 写入”作为一个原子步骤，不等于整个业务写入天然完整 |
| `O_CLOEXEC` | 执行新程序时关闭此 fd | 多线程程序应在 open 时设置，避免 open 后再 fcntl 的竞态窗口 |
| `O_NONBLOCK` | 不能立即完成时返回，而不是一直等待 | `EAGAIN/EWOULDBLOCK` 是暂时不可读写，不是文件结束 |
| `O_DIRECT` | 尽量减少页缓存影响，直接使用用户缓冲区做 IO | 地址、长度、偏移可能有对齐要求，不适合拿来无脑“提速” |

`open` 失败返回 `-1` 并设置 `errno`，不能把它当正常 fd 继续使用。`close` 也不是可省略的“收尾礼节”：fd 泄漏最终会导致进程无法打开新文件。

### 2.2 read/write 的返回值必须分类处理

| 返回值 | read 的含义 | write 的含义 | 正确处理 |
|---|---|---|---|
| `> 0` | 实际读到的字节数 | 实际写出的字节数 | 只能按这个长度处理；写操作继续补写剩余部分 |
| `0` | 对普通文件通常表示 EOF | 请求长度非 0 时不应把它当成功完成 | 读结束；写需避免死循环并检查异常 |
| `-1` 且 `errno == EINTR` | 在读到数据前被信号打断 | 在写出数据前被信号打断 | 通常重试 |
| `-1` 且 `EAGAIN/EWOULDBLOCK` | 非阻塞 fd 当前无数据 | 非阻塞 fd 当前写不下 | 等 fd 就绪后再试，不是 EOF |
| `-1` 且其他 errno | 真正失败 | 真正失败 | 记录错误并进入清理流程 |

**核心规则：`read` 不保证一次读满，`write` 也不保证一次写完。** 管道、socket 和信号中断让部分读写更常见；普通文件也不能省略返回值检查。

### 2.3 一个带完整错误处理的文件复制程序

```c
// copy_file.c
// 编译：gcc -std=c11 -Wall -Wextra -O2 copy_file.c -o copy_file
// 运行：./copy_file source.txt backup.txt
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

// 把 count 字节全部写完；遇到短写或 EINTR 都继续处理。
static int write_all(int fd, const char *buf, size_t count) {
    size_t done = 0;

    while (done < count) {
        ssize_t n = write(fd, buf + done, count - done);
        if (n > 0) {
            done += (size_t)n; // 只累加本次真正写出的字节数
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue; // 尚未写出数据就被信号打断，可以重试
        }
        if (n == 0) {
            errno = EIO; // 防止剩余数据非零时原地死循环
        }
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char buffer[4096];
    int failed = 0;

    if (argc != 3) {
        fprintf(stderr, "用法：%s <源文件> <目标文件>\n", argv[0]);
        return 1;
    }

    int input = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (input == -1) {
        perror("open 源文件");
        return 1;
    }

    int output = open(argv[2],
                      O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                      0644); // 只有 O_CREAT 创建文件时才使用这个权限参数
    if (output == -1) {
        perror("open 目标文件");
        close(input);
        return 1;
    }

    for (;;) {
        ssize_t n = read(input, buffer, sizeof(buffer));
        if (n > 0) {
            if (write_all(output, buffer, (size_t)n) == -1) {
                perror("write");
                failed = 1;
                break;
            }
            continue;
        }
        if (n == 0) {
            break; // 读到 EOF，正常结束
        }
        if (errno == EINTR) {
            continue; // read 尚未读到数据就被信号打断
        }
        perror("read");
        failed = 1;
        break;
    }

    // 某些写回错误可能延迟到 close 才报告，因此关闭文件也要看结果。
    if (close(output) == -1) {
        perror("close 目标文件");
        failed = 1;
    }
    if (close(input) == -1) {
        perror("close 源文件");
        failed = 1;
    }
    return failed;
}
```

这个示例处理的是“复制过程正确”，还没有保证“目标文件永远处于完整状态”。生产程序若怕中途失败留下半个文件，通常先写同目录临时文件，检查 `fsync` 和 `close` 成功后再原子 `rename`。

---

## 三、文件偏移量与 pread/pwrite

对支持定位的普通文件，`read/write` 从当前文件偏移量开始，并按实际传输字节数推进偏移量。`lseek` 可以显式调整它。

```c
off_t current = lseek(fd, 0, SEEK_CUR); // 查询当前位置
off_t begin   = lseek(fd, 0, SEEK_SET); // 回到文件开头
off_t end     = lseek(fd, 0, SEEK_END); // 移到 EOF，返回值也可得到文件大小
```

`pread/pwrite` 则多传一个 offset：在指定位置读写，**不改变打开文件描述上的当前偏移量**。这在多线程随机读写同一个 fd 时尤其有用。

| 方式 | 是否使用共享文件偏移量 | 多线程随机读写时的风险 |
|---|---|---|
| `lseek + read/write` | 是 | 两个调用之间可能被另一线程插入操作，偏移被改掉 |
| `pread/pwrite` | 否，使用调用参数里的 offset | 避免“定位—读写”组合操作的偏移竞态 |
| 每个线程独立 open | 各自打开文件描述有独立偏移 | fd 和打开文件描述更多，但隔离更直观 |

注意：`pread/pwrite` 解决的是偏移量竞态，不会自动把多个业务写操作合成一个事务，也不代表任意长度、任意文件系统上的写入都具有原子性。

---

## 四、缓冲区：write 返回不等于数据已经落盘

一次看似简单的 `fwrite`，中间可能经过多层缓冲：

```text
应用内存
   │  fwrite：先放进程内的 stdio 缓冲区
   ▼
stdio 缓冲区 ── fflush / 缓冲区满 / 关闭流 ──→ write 系统调用
   │
   ▼
内核页缓存（page cache）
   │  内核在合适时机写回；fsync 可主动请求同步
   ▼
存储设备及其缓存
```

| 操作 | 能保证到哪里 | 不能保证什么 |
|---|---|---|
| `printf/fwrite` 返回 | 数据已被 stdio 接收，或已触发相应写出 | 不一定已经调用 write，更不一定落盘 |
| `fflush(FILE *)` | 把该 stdio 流的输出缓冲推给内核 | 不保证内核页缓存已写入存储设备 |
| `write` 成功返回 | 内核已接收相应数据，后续符合语义的 read 可读到 | 不保证掉电后数据仍在 |
| `fsync(fd)` 成功 | 将该文件相关数据及必要元数据推向持久化完成点 | 不代表系统中其他文件也同步，也不消除硬件与文件系统差异 |

普通读命中页缓存时，甚至不需要访问磁盘。页缓存能明显提速，但“缓存里的新数据”和“持久化数据”是两个概念。

还有一个容易忽略的点：**stdio 流和 fd 是两套缓冲视角。** 同一个文件上混用 `fprintf/fread` 与 `write/read`，如果没有按标准做好刷新或定位协调，顺序可能与预期不同。实际项目最好选择一套接口，不要交叉操作同一底层文件。

---

## 五、fsync、fdatasync 与 O_SYNC：持久化到底怎么做

数据库、账本和配置更新关心的往往不是“现在能不能读到”，而是“进程崩溃或机器掉电后还能不能恢复”。这时要明确同步边界。

| 方式 | 同步时机与范围 | 特点 |
|---|---|---|
| `fsync(fd)` | 调用时同步指定文件的数据和完成数据取回所需的元数据 | 常用持久化点，可能带来明显延迟 |
| `fdatasync(fd)` | 类似 fsync，但只要求数据及后续取回数据所必需的元数据 | 可能减少无关元数据写入，不等于永远更快 |
| `O_SYNC` | 每次相关 write 都按同步文件完整性要求完成 | 语义强，频繁小写时开销通常更大 |
| `sync()` | 请求把系统范围内待写回数据调度同步 | 粒度过大，业务代码通常不用它替代单文件 fsync |

一个关键写入流程应像这样检查错误：

```c
// 假设 fd 已用可写方式打开，message 是需要保存的完整内容。
if (write_all(fd, message, message_length) == -1) {
    perror("write");
    // 进入清理或失败恢复流程
}
if (fsync(fd) == -1) {
    perror("fsync");
    // 不能把这次保存报告为持久化成功
}
if (close(fd) == -1) {
    perror("close");
    // 即使这里失败，也不能忽略错误
}
```

如果文件是刚创建的，还要求文件名对应的目录项在崩溃后存在，除了同步文件，往往还要按文件系统语义同步其父目录。若采用“临时文件 + rename”发布新配置，还要把临时文件同步、目录同步和 rename 顺序一起设计。

不要把 `fsync` 当作每条日志都必调用的固定答案：同步越频繁，延迟和写放大通常越明显。正确做法是根据可接受的数据丢失窗口，设计单条同步、分组提交或异步刷盘。

---

## 六、mmap：把文件映射进虚拟内存

`read` 的模型是“请内核把字节复制到我的缓冲区”；`mmap` 的模型是“把文件的一段映射到进程虚拟地址空间，之后像访问内存一样访问文件内容”。

```text
虚拟地址 [data, data + length)
              │ 首次访问某页时触发缺页
              ▼
         建立页表映射
              │
              ▼
       文件对应的页缓存页
```

### 6.1 一个可直接运行的 mmap 读取示例

```c
// mmap_read.c
// 编译：gcc -std=c11 -Wall -Wextra -O2 mmap_read.c -o mmap_read
// 运行：./mmap_read source.txt
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法：%s <文件>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct stat info;
    if (fstat(fd, &info) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }
    if (info.st_size == 0) {
        close(fd); // length 为 0 不能建立 mmap，这里直接输出空文件
        return 0;
    }

    size_t length = (size_t)info.st_size;
    char *data = mmap(NULL, length, PROT_READ, MAP_PRIVATE, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // 映射建立后即可关闭 fd；映射本身仍然有效。
    if (close(fd) == -1) {
        perror("close");
        munmap(data, length);
        return 1;
    }

    // 使用长度而不是 strlen，二进制文件中含 '\0' 也能完整输出。
    if (fwrite(data, 1, length, stdout) != length) {
        perror("fwrite");
        munmap(data, length);
        return 1;
    }

    if (munmap(data, length) == -1) {
        perror("munmap");
        return 1;
    }
    return 0;
}
```

### 6.2 MAP_SHARED 与 MAP_PRIVATE

| 对比 | `MAP_SHARED` | `MAP_PRIVATE` |
|---|---|---|
| 对映射的修改 | 会反映到文件映射，其他映射同一区域的人可见 | 写时复制，修改只留在本进程私有页 |
| 是否写回底层文件 | 会，精确时机可用 `msync` 控制 | 不会写回底层文件 |
| 常见用途 | 文件内容更新、进程间共享文件映射 | 只读访问、进程内临时修改 |
| 权限要求 | `PROT_WRITE` 通常要求 fd 以可读写方式打开 | 私有写映射不要求把修改写回原文件 |

### 6.3 mmap 的底层收益与代价

- **少一次显式数据复制**：文件页可通过页缓存映射给进程，不必为 read 再复制到应用缓冲区；
- **按需调页**：不是调用 mmap 就把整个文件读入 RAM，首次访问页面时才处理缺页；
- **页缓存可共享**：多个进程映射同一文件时，可复用相同文件页；
- **缺页也有成本**：随机访问大文件时，缺页、页表和 TLB 开销可能抵消收益；
- **错误表现不同**：read 用返回值报告很多错误，映射后访问问题可能表现为 `SIGBUS/SIGSEGV`。

特别注意：如果别的进程把文件截短，再访问已经映射但超出新文件末尾的页面，可能收到 `SIGBUS`。mmap 不是把文件内容做了永久快照。

---

## 七、零拷贝：sendfile 为什么能减少开销

传统“从磁盘文件读，再写到 socket”的路径大致是：

```text
磁盘 → 内核页缓存 → 用户 read 缓冲区 → 内核 socket 缓冲区 → 网卡
              read 复制       write 复制
```

数据先进入用户态，又马上原样交给内核，用户程序并没有加工它。`sendfile` 让传输在内核内部完成，避免数据往返用户空间，同时减少系统调用和上下文切换。

```text
应用程序调用 sendfile(out_fd, in_fd, ...)
              │
              ▼
内核在文件与输出对象之间传递数据，不经过用户中转缓冲区
```

“零拷贝”不是说整个硬件路径上一次复制都不存在，而是强调**消除不必要的用户态中转复制**。具体实现是否还能进一步利用 DMA、页引用等优化，取决于内核、文件类型和设备。

### 7.1 sendfile 示例：文件到文件

```c
// sendfile_copy.c —— Linux 专用示例
// 编译：gcc -std=c11 -Wall -Wextra -O2 sendfile_copy.c -o sendfile_copy
// 运行：./sendfile_copy source.txt target.txt
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法：%s <源文件> <目标文件>\n", argv[0]);
        return 1;
    }

    int input = open(argv[1], O_RDONLY | O_CLOEXEC);
    if (input == -1) {
        perror("open 源文件");
        return 1;
    }

    struct stat info;
    if (fstat(input, &info) == -1) {
        perror("fstat");
        close(input);
        return 1;
    }

    int output = open(argv[2],
                      O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                      0644);
    if (output == -1) {
        perror("open 目标文件");
        close(input);
        return 1;
    }

    off_t offset = 0; // 非 NULL：sendfile 更新它，但不改 input 的当前文件偏移
    off_t remaining = info.st_size;
    int failed = 0;

    while (remaining > 0) {
        // 限制单次请求大小；即使请求很大，也要按返回值处理短写。
        size_t chunk = remaining > 0x40000000 ? 0x40000000 : (size_t)remaining;
        ssize_t n = sendfile(output, input, &offset, chunk);
        if (n > 0) {
            remaining -= n;
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n == 0) {
            errno = EIO; // 尚未传完却返回 0，不能当作完整成功
        }
        perror("sendfile");
        failed = 1;
        break;
    }

    if (close(output) == -1) {
        perror("close 目标文件");
        failed = 1;
    }
    if (close(input) == -1) {
        perror("close 源文件");
        failed = 1;
    }
    return failed;
}
```

`sendfile` 也可能短写，也必须循环处理。`offset != NULL` 时它使用并更新这个独立偏移，不修改输入 fd 的当前偏移；传 `NULL` 时才从并更新输入 fd 的当前偏移。

| 技术 | 典型用途 | 一句话理解 |
|---|---|---|
| `sendfile` | 文件内容直接发往 socket，也可用于支持的文件间传输 | 在内核中完成搬运，省去用户缓冲区中转 |
| `splice` | 借助管道在内核对象之间移动数据 | 更通用的 Linux 数据拼接接口，使用条件要看 fd 类型 |
| `copy_file_range` | 同内核中的文件范围复制 | 避免用户态中转，并可能利用文件系统加速能力 |
| `mmap + write` | 映射读取后再写另一个 fd | 可减少 read 的复制，但不等于天然完成零拷贝发送 |

如果应用必须修改文件内容，例如加协议头、压缩、加密或按业务过滤，就不能只凭“零拷贝更快”决定架构。数据需要在哪里加工，决定了用户缓冲区是否仍然必要。

---

## 八、工程选型：没有一种 IO 永远最快

| 场景 | 优先考虑 | 原因与注意事项 |
|---|---|---|
| 小文件顺序复制 | 较大的用户缓冲区 + read/write | 简单可靠，系统调用次数可控 |
| 大文件顺序读取 | read 或 mmap 都可，实测选择 | read 可利用预读；mmap 可减少复制但有缺页成本 |
| 大文件随机只读访问 | mmap | 按页访问直观，但要评估缺页、TLB 和文件截断风险 |
| 多线程固定位置读写 | pread/pwrite | 避免共享文件偏移量的 lseek 竞态 |
| 静态文件直接发 socket | sendfile | 无需用户态中转，但要处理短写和不支持时的回退 |
| 需要修改内容的转发 | read/write、缓冲区复用或专用异步 IO | 数据必须加工，不能机械追求零拷贝 |
| 自带缓存的存储引擎 | 评估 O_DIRECT | 避免双重缓存前必须确认对齐、持久化和性能特征 |
| 崩溃后不能丢的数据 | write + fsync 等明确同步协议 | 性能优化不能绕过持久化边界 |

`O_DIRECT` 尤其容易被误用：它不是“绕过缓存所以必快”，反而常因小块 IO、缺少预读和对齐限制而变慢。只有在应用自己做好缓存、批处理和预读时，才值得用基准测试验证。

性能比较时应固定文件大小、冷热缓存、块大小、磁盘类型和并发度。一次本机测试不能推出所有环境下的通用结论。

---

## 面试真题

### 真题 1：fd、打开文件描述和 inode 是什么关系？

三句话答案：fd 是进程 fd 表里的整数句柄，不是文件内容本身。fd 指向内核的打开文件描述，后者保存文件偏移量和状态标志，再指向 inode。dup 或 fork 继承的 fd 可以共享打开文件描述，而独立 open 通常得到不同的打开文件描述。

### 真题 2：为什么不能假设 read/write 一次完成全部数据？

三句话答案：它们的成功返回值是实际传输字节数，可以小于请求长度。信号、可用数据量、管道或 socket 状态等都可能导致部分读写。正确代码要按返回值推进缓冲区，并对 EINTR 和 EAGAIN 分别处理。

### 真题 3：EINTR 和 EAGAIN 有什么区别？

三句话答案：EINTR 表示调用在传输数据前被信号中断，阻塞 IO 中通常可以重试。EAGAIN 或 EWOULDBLOCK 表示非阻塞 fd 当前不能完成操作，应等待合适就绪事件后再试。EAGAIN 不是 EOF，也不意味着连接一定坏了。

### 真题 4：write 返回成功，数据就一定落盘了吗？

三句话答案：不一定，成功返回通常只说明内核已经接收了相应数据。数据还可能留在页缓存或存储路径的缓存中，部分错误也可能延迟到后续 write、fsync 或 close 才暴露。需要明确持久化保证时，应调用并检查 fsync，必要时还要处理父目录同步。

### 真题 5：fflush 和 fsync 有什么区别？

三句话答案：fflush 作用于 C 标准库的 FILE 流，把用户态 stdio 输出缓冲推给内核。fsync 作用于 fd，请求内核把指定文件的数据及必要元数据同步到持久化完成点。只调用 fflush 不能保证掉电后的数据持久性。

### 真题 6：pread/pwrite 解决了什么问题？

三句话答案：它们在调用参数指定的位置读写，不修改共享的当前文件偏移量。多线程若共用 fd，lseek 和 read/write 分开调用可能出现偏移被别的线程改变的竞态。pread/pwrite 消除了这组定位和读写之间的偏移竞态，但不自动提供业务事务原子性。

### 真题 7：mmap 为什么可能比 read 快？

三句话答案：mmap 把文件页映射到虚拟地址空间，访问时可直接使用页缓存，减少显式复制到 read 缓冲区的过程。它还能按需调页，并让多个映射复用文件页。但随机访问下的缺页、页表和 TLB 成本也不低，所以是否更快必须按访问模式实测。

### 真题 8：MAP_SHARED 和 MAP_PRIVATE 有什么区别？

三句话答案：MAP_SHARED 的修改会反映到文件映射，并可被映射同一区域的其他进程看到。MAP_PRIVATE 使用写时复制，写入只形成本进程私有页，不写回底层文件。需要控制共享映射写回时机时，还应结合 msync。

### 真题 9：零拷贝真的一次复制都没有吗？

三句话答案：不是，它主要表示消除数据在用户缓冲区和内核之间不必要的中转复制。sendfile 能让文件到输出的传输在内核内完成，并减少系统调用和上下文切换。底层仍可能存在页缓存、协议栈和 DMA 等数据移动，具体取决于实现。

### 真题 10：O_DIRECT 为什么不一定更快？

三句话答案：O_DIRECT 减少了页缓存的参与，但也可能失去内核缓存和预读带来的收益。它还常对用户缓冲区地址、IO 长度和文件偏移提出对齐要求。只有应用自己能做好缓存、批处理和预读时，才值得通过基准测试评估。

---

## 踩坑提醒

1. **用 int 保存 read/write 返回值**：实际返回类型是 `ssize_t`，`-1` 和短读写都要正确处理。
2. **write 后直接按 count 处理**：写出长度可能小于 count，必须从已写位置继续补写。
3. **把 read 返回 0 当成暂时没数据**：对普通文件通常表示 EOF；非阻塞暂时无数据应看 EAGAIN。
4. **EINTR 一律当失败退出**：尚未传输数据时被信号打断通常可以重试。
5. **O_CREAT 忘记 mode**：创建文件时必须提供权限参数，最终权限还受 umask 影响。
6. **混用 stdio 与 fd IO**：两套缓冲和偏移协调容易导致输出顺序或读取位置不符合预期。
7. **认为 close 等于 fsync**：close 会关闭句柄，但不提供与 fsync 相同的持久化保证。
8. **用 NULL 判断 mmap 失败**：失败返回值是 `MAP_FAILED`，即 `(void *)-1`。
9. **mmap offset 不按要求对齐**：文件映射偏移必须是系统页大小的整数倍。
10. **munmap 后继续访问指针**：映射已失效，继续解引用可能导致 SIGSEGV。
11. **把 sendfile 一次调用当完整发送**：它同样可能短写，循环和错误处理不能省。
12. **为了压测绕过缓存却不说明条件**：冷缓存和热缓存结果差异巨大，O_DIRECT 测试也不代表普通业务路径。

---

## 参考资料

本章为原创整理，示例代码为独立编写；仅把以下公开资料用于核对系统调用语义、边界条件与高频考点，未复制原文：

- Linux man-pages：open(2)：https://man7.org/linux/man-pages/man2/open.2.html
- Linux man-pages：read(2)：https://man7.org/linux/man-pages/man2/read.2.html
- Linux man-pages：write(2)：https://man7.org/linux/man-pages/man2/write.2.html
- Linux man-pages：mmap(2)：https://man7.org/linux/man-pages/man2/mmap.2.html
- Linux man-pages：sendfile(2)：https://man7.org/linux/man-pages/man2/sendfile.2.html

---

## 上一节 | 下一节

[4.1 Linux 基础与排障命令](../01-basics/README.md) | 4.3 进程与线程（Linux 视角）（待写）

---

> 本教程由**青松与桑叶**编写，面向 C/C++、Linux 后端、嵌入式与系统软件岗位面试的保姆级中文教学。
