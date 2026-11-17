# 4.1 Linux 基础与排障命令

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

---

## 目录

- 一、先建立一张 Linux 地图
- 二、一切皆文件：inode 才是文件的真身
- 三、权限：rwx 到底管的是什么
- 四、软链接与硬链接：面试最爱问的一组对比
- 五、进程、信号与僵尸
- 六、排障五件套：CPU、内存、磁盘、网络、日志
- 七、管道、重定向与后台运行
- 八、面试真题
- 九、踩坑提醒

---

## 一、先建立一张 Linux 地图

面试官问「你熟悉 Linux 吗」，多半不是要你背命令，而是想看你对**结构**有没有概念。先把这张图记住：

| 层 | 内容 | 你日常接触的东西 |
|---|---|---|
| 硬件 | CPU、内存、磁盘、网卡 | `lscpu`、`free`、`df`、`ip` |
| 内核 | 进程调度、内存管理、VFS、网络协议栈、驱动 | `dmesg`、`/proc`、`/sys` |
| 系统调用 | `open/read/write/fork/execve` 等几百个入口 | `strace`、`man 2 read` |
| 用户态 | shell、服务、库 | `bash`、`systemd`/`openrc`、`nginx` |

两个高频结论先说在前面：

1. **Linux 不区分「扩展名」**，`file` 命令看的是文件头（magic number），不是 `.txt`；
2. **配置和状态分家**：`/etc` 放配置，`/var` 放运行数据（日志在 `/var/log`），`/proc` 和 `/sys` 是内核暴露的虚拟文件，**不占磁盘**，读写它们等于直接问内核。

```bash
# 只读，随便跑
uname -a            # 内核版本
cat /etc/os-release # 发行版
ls /proc/self/      # 当前 shell 进程的内核视图
```

---

## 二、一切皆文件：inode 才是文件的真身

这句话面试常考，但多数人只会背。把下面这组关系说清楚就够了：

```
目录项（dentry）：  "report.txt"  →  inode 12345
inode：           权限 / 属主 / 大小 / 时间戳 / 数据块指针（不含文件名！）
数据块：           真正的文件内容
```

**文件名不是文件的一部分，它只是目录里的一条映射记录。** 这解释了三件事：

1. 同一个 inode 可以有多个名字——这就是硬链接；
2. 删文件本质是「删目录项 + 链接计数减一」，计数归零才真正释放数据块；
3. 进程打开文件后，即使文件被 `rm`，只要 fd 还在，数据就还在（`ls -l /proc/<pid>/fd` 能看到 `(deleted)`）。

```bash
stat report.txt      # 看 inode 号、链接计数、三个时间戳
ls -i report.txt     # 只取 inode 号
df -i                # 看 inode 使用率（满了也是「磁盘没满却写不进去」的经典原因）
```

> 面试点：`df` 显示还有空间但创建文件报 `No space left on device`，第一反应应该是 `df -i` 查 inode 是否耗尽（常见于海量小文件、缓存目录）。

三个时间戳也别混：`atime` 访问、`mtime` 内容修改、`ctime` **inode 变更**（改权限、改属主、改名都会动 ctime）。

---

## 三、权限：rwx 到底管的是什么

`ls -l` 的第一列有 10 位，拆开读：

```
 -   rwx   r-x   r-x
 │    │     │     │
 │    │     │     └─ other（其他人）
 │    │     └─────── group（同组）
 │    └───────────── owner（属主）
 └────────────────── 类型：- 普通文件，d 目录，l 软链接，s socket，p 管道，c/b 设备
```

数字表示法：`r=4, w=2, x=1`，三组相加，所以 `chmod 755` 等于 `rwx r-x r-x`。

**最容易答错的一点：目录上的 `x` 不是「执行」，而是「能进入 / 能遍历」。**

| 目录权限 | 实际含义 |
|---|---|
| `r` | 能列出目录里有哪些名字（`ls`） |
| `x` | 能进入目录、能访问里面的文件（`cd`、读文件路径） |
| `w` | 能在目录里创建 / 删除 / 重命名条目 |
| `rx` | 最常见的组合：能看能进，不能改 |

所以「目录只有 `r` 没有 `x`」时，`ls` 能看到文件名，但 `cat 目录/文件` 会 Permission denied——这个组合是面试常考的细节。

### 3.1 三个特殊位

| 位 | 数字 | 作用对象 | 效果 |
|---|---|---|---|
| SUID | 4 | 可执行文件 | 执行时以**属主**身份运行（如 `/usr/bin/passwd`） |
| SGID | 2 | 目录 | 目录里新建的文件**继承目录的组** |
| Sticky | 1 | 目录 | 只有属主能删自己创建的文件（`/tmp` 就是 `1777`） |

```bash
chmod u+s app          # 加 SUID
chmod 2755 shared_dir  # SGID + 常规权限
chmod o+t /tmp         # Sticky
```

### 3.2 umask：新建文件的默认权限怎么来的

```
文件最终权限 = 0666 & ~umask
目录最终权限 = 0777 & ~umask
```

`umask 022` 时：文件 644、目录 755。`umask 077` 时：文件 600、目录 700（私密场景常用）。

> 注意：`umask` 是**掩码**不是权限值，它表示「要抹掉哪些位」。面试时说「umask 022 表示去掉组和其他人的写权限」比说「默认权限是 644」更准确。

---

## 四、软链接与硬链接：面试最爱问的一组对比

| 维度 | 硬链接 | 软链接（符号链接） |
|---|---|---|
| inode | 与源文件**同一个** | **独立**的 inode |
| 本质 | 多一条目录项 | 一个存着目标路径的特殊文件 |
| 跨文件系统 | ❌ 不行 | ✅ 可以 |
| 指向目录 | ❌ 普通用户不行 | ✅ 可以 |
| 源文件删除后 | 仍可访问（链接计数 > 0） | 变成悬空链接（dangling） |
| 大小 | 与源文件相同 | 等于目标路径字符串长度 |
| 创建 | `ln src dst` | `ln -s src dst` |

```bash
ln target.txt hard.txt     # 硬链接：ls -l 里链接计数 +1
ln -s /data/app/conf app   # 软链接：ls -l 显示 -> /data/app/conf
```

两个必背结论：

1. **硬链接不能跨分区**，因为它共享 inode，而 inode 编号只在单个文件系统内唯一；
2. **删除源文件不影响硬链接的数据**，只是链接计数减一，归零才释放；软链接则立刻失效。

---

## 五、进程、信号与僵尸

### 5.1 进程的基本视图

```bash
ps -ef            # 全量，含 PPID（父进程）
ps aux            # 带 CPU/内存占用
top -H            # 进线程视图（大写 H）
pgrep -af server  # 按命令行匹配 PID，比 ps | grep 干净
```

每个进程在 `/proc/<pid>/` 下有一整套视图：

| 路径 | 内容 |
|---|---|
| `/proc/<pid>/status` | 状态、PPID、内存、线程数 |
| `/proc/<pid>/cmdline` | 启动命令行（`\0` 分隔） |
| `/proc/<pid>/fd/` | 打开的文件描述符 |
| `/proc/<pid>/cwd` | 工作目录 |
| `/proc/<pid>/environ` | 环境变量 |

### 5.2 一个 fork 例子看懂父子进程

```c
// fork_demo.c —— gcc -Wall -o fork_demo fork_demo.c && ./fork_demo
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();              // 调用一次，返回两次
    if (pid < 0) { perror("fork"); return 1; }

    if (pid == 0) {                  // 子进程分支：返回 0
        printf("child: pid=%d ppid=%d\n", getpid(), getppid());
        _exit(0);                    // 用 _exit 避免刷新父进程的 stdio 缓冲区
    }

    printf("parent: pid=%d child=%d\n", getpid(), pid);
    wait(NULL);                      // 回收子进程，否则子进程变成僵尸
    return 0;
}
```

要点：

- `fork` 之后父子进程**各自返回一次**，子进程拿到 0，父进程拿到子 PID；
- 子进程是父进程地址空间的副本，内核用**写时复制（COW）**延迟真实拷贝；
- `_exit` 直接进内核，`exit` 会先刷新 stdio 缓冲——在 fork 后的子进程里用 `exit` 可能把父进程的缓冲重复输出一遍，这是经典面试题；
- 父进程不 `wait`，子进程退出后残留的 PCB 就是**僵尸进程**（状态 `Z`），`kill -9` 杀不掉，只能让父进程回收或杀掉父进程。

### 5.3 信号：能捕获的和不能捕获的

| 信号 | 编号 | 典型用途 | 能捕获吗 |
|---|---|---|---|
| `SIGHUP` | 1 | 终端断开、很多服务用它重载配置 | ✅ |
| `SIGINT` | 2 | Ctrl+C | ✅ |
| `SIGKILL` | 9 | 强杀 | ❌ **不可捕获、不可忽略** |
| `SIGSEGV` | 11 | 段错误 | ✅（但通常不建议恢复） |
| `SIGTERM` | 15 | 默认终止信号，`kill` 默认就是它 | ✅ |
| `SIGCHLD` | 17 | 子进程状态变化，父进程据此回收 | ✅ |

```bash
kill -TERM <pid>   # 先礼后兵：给进程机会清理资源
kill -9 <pid>      # 最后手段：进程没有任何清理机会
```

> 面试点：为什么「先 TERM 后 KILL」？因为 SIGTERM 能被捕获，进程可以落盘、关连接、删临时文件；SIGKILL 由内核直接终止，可能留下半截文件和没释放的共享内存。

---

## 六、排障五件套：CPU、内存、磁盘、网络、日志

线上出问题，面试官喜欢问「你怎么定位」。按这五层走，答得就有条理。

### 6.1 CPU

```bash
uptime            # 看 load average：1/5/15 分钟平均值
top -P            # 按 CPU 排序
pidstat -u 1 5    # 单进程 CPU 明细（sysstat 包）
```

**load average 的坑**：它统计的是「运行中 + 不可中断睡眠（D 状态）」的进程数，所以磁盘 IO 卡死时 load 会飙高，但 CPU 可能很闲。答「load 高就是 CPU 忙」会被追问。

经验值：load 长期超过**逻辑核数**就要看；超过核数的 2 倍基本已经影响响应。

### 6.2 内存

```bash
free -h           # 重点看 available，不是 free
cat /proc/meminfo | head
```

`free` 数值小不用慌，Linux 会把空闲内存拿去做 page cache，真正要看 `available`（可回收后可用的估算值）。

OOM 排查：

```bash
dmesg -T | grep -i 'out of memory'   # 内核 OOM killer 记录
```

> 面试点：容器里 `free` 看到的是宿主机内存，不看 cgroup 限制会误判；容器限额在 `/sys/fs/cgroup/memory.max`（cgroup v2）。

### 6.3 磁盘

```bash
df -h          # 各分区空间
df -i          # inode
du -sh /var/*  # 逐目录定位大块头
lsof +L1       # 找出「已删除但仍被进程占用」的大文件
```

最后一条是经典救火命令：日志文件被 `rm` 了但空间没释放，因为进程还开着 fd，`lsof +L1` 能揪出来，重启进程或 `> /proc/<pid>/fd/<n>` 清空即可。

### 6.4 网络

```bash
ss -ltnp                 # 谁在监听、对应哪个进程
ss -tnp state established  # 已建立的连接
ss -s                   # 连接数汇总
lsof -i:8080            # 按端口找进程
tcpdump -i any -n port 8080 -c 20   # 抓 20 个包，只读操作
```

### 6.5 日志

```bash
tail -f /var/log/app.log
grep -n 'ERROR' app.log | tail -50
journalctl -u nginx --since '10 min ago'   # systemd 系统
```

排障顺序建议固定成一句话：**先看现象（谁慢/谁挂）→ 再看资源（CPU/内存/磁盘/网络）→ 最后看日志和堆栈**。面试时按这个顺序讲，比零散报命令名更得分。

---

## 七、管道、重定向与后台运行

```bash
cmd > out.txt          # 覆盖标准输出
cmd >> out.txt         # 追加
cmd 2> err.txt         # 只重定向标准错误
cmd > all.txt 2>&1     # 标准错误并入标准输出（顺序：先复制 fd 再重定向）
cmd 2>&1 | tee all.log # 屏幕和文件同时留一份
```

`2>&1` 的写法顺序不能反：`> file 2>&1` 表示「先把 stdout 指到 file，再让 stderr 指向 stdout 当前的位置」，所以两者都进 file；写成 `2>&1 > file` 则 stderr 仍指向原终端。

后台运行与保活：

```bash
nohup ./server > server.log 2>&1 &   # 退出终端不中断
disown -h %1                         # 已启动的作业脱离 shell
```

生产环境不要长期靠 `nohup`：用 systemd（`systemctl enable --now`）或容器编排，才有崩溃重启和开机自启。

---

## 八、面试真题

### 真题 1：硬链接和软链接的区别？

硬链接共享同一个 inode，只是多一条目录项，不能跨文件系统、不能指向目录，删源文件数据仍在；软链接是独立 inode，内容为目标路径，可跨文件系统、可指向目录，源文件删掉后变悬空链接。

### 真题 2：`df` 有空间但写文件失败，为什么？

先看 `df -i`：inode 耗尽时即便块还有空间也无法创建新文件。其次看是否命中了「已删除但仍被进程占用」的文件（`lsof +L1`），以及是否有磁盘配额（`quota`）或只读挂载。

### 真题 3：目录权限 777 和 755 的实际差别？

755 表示其他人只能读和进入，不能在目录里增删；777 允许任何人创建/删除文件。对 Web 目录给 777 是典型的安全隐患，正确做法是属主可写、Web 进程只读，需要上传目录时单独授权。

### 真题 4：僵尸进程怎么产生、怎么处理？

子进程先于父进程退出，父进程没有 `wait/waitpid` 回收，残留的 PCB 就是僵尸（`Z`）。`kill -9` 对僵尸无效，因为它已经死了。处理方式：让父进程调用 `wait`，或给父进程装 `SIGCHLD` 处理器；父进程本身有问题就重启/杀掉父进程，僵尸会被 init 收养并回收。

### 真题 5：`kill -9` 和 `kill -15` 的区别？

15 是 SIGTERM，可被捕获，进程能做清理；9 是 SIGKILL，不可捕获不可忽略，由内核直接终止。规范做法是先 TERM，等待超时后再 KILL。

### 真题 6：load average 高但 CPU 空闲，可能是什么原因？

大量进程处于 D 状态（不可中断睡眠），通常是磁盘 IO 或网络文件系统卡住；也可能是内存不足导致频繁换页。load 统计包含 D 状态，所以会出现「load 高、CPU 闲」。

### 真题 7：怎么找出占用 8080 端口的进程？

`ss -ltnp | grep :8080` 或 `lsof -i:8080`；拿不到进程信息通常是权限不足（非 root）。

### 真题 8：`> file 2>&1` 和 `2>&1 > file` 有什么区别？

前者 stdout 和 stderr 都进文件；后者先把 stderr 复制到当前 stdout（终端），再把 stdout 重定向到文件，所以错误仍打在屏幕上。

### 真题 9：怎么让程序在退出 SSH 后继续运行？

临时用 `nohup cmd &` 或 `tmux/screen`；长期用 systemd 单元或容器，并配置重启策略。

### 真题 10：`/proc` 是什么文件系统？

伪文件系统（procfs），由内核动态生成，不占磁盘。它把进程、内核参数、设备信息以文件形式暴露，读写对应内核操作，是排查和调优的主要入口（如 `/proc/sys/...`）。

---

## 九、踩坑提醒

1. **`rm -rf` 前先确认变量**：`rm -rf $DIR/` 在 `DIR` 为空时会变成 `rm -rf /`，脚本里必须写 `set -u` 并校验非空。
2. **`chmod -R 777` 是懒办法**，会把 SUID 等特殊位一起改乱，且埋下提权风险。
3. **不要在 `/etc` 里手改后不验证**：改完配置先跑 `nginx -t` 之类的语法检查，再 reload 而不是 restart。
4. **`tail -f` + `grep` 会缓冲**：管道给 grep 时加 `--line-buffered`，否则看起来像「日志不更新」。
5. **容器里的 `ps` 不全**：容器默认看不到宿主机进程，排查宿主机问题要进宿主机命名空间。

---

## 参考资料

本章为原创整理，知识点覆盖对照了以下公开资料，未复制原文：

- Linux man-pages（`man 2 open`、`man 7 signal`、`man 1 stat`）：<https://man7.org/linux/man-pages/>
- The Linux Documentation Project（TLDP）：<https://tldp.org/>
- 《深入理解计算机系统》第 9–10 章（虚拟内存与系统级 IO）
- 《Unix 环境高级编程》第 3–4 章（文件 IO 与文件系统）

---

## 上一节 | 下一节

[3.5 多线程编程](../../03-cpp-advanced/concurrency/README.md)（待写） | 4.2 文件 IO 与系统调用（待写）

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 与 Linux 开发岗位面试的保姆级中文教学。
