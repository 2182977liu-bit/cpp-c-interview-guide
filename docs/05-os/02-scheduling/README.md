# 5.2 进程调度

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

## 目录

- [1. 调度器到底在解决什么问题](#1-调度器到底在解决什么问题)
- [2. 什么时候会发生调度](#2-什么时候会发生调度)
- [3. 上下文切换：切了什么，贵在哪里](#3-上下文切换切了什么贵在哪里)
- [4. 自愿切换与非自愿切换](#4-自愿切换与非自愿切换)
- [5. Linux 调度类：先分层，再谈公平](#5-linux-调度类先分层再谈公平)
- [6. CFS：虚拟运行时间与红黑树](#6-cfs虚拟运行时间与红黑树)
- [7. EEVDF：内核 6.6 之后的新答案](#7-eevdf内核-66-之后的新答案)
- [8. nice 值：普通任务之间的权重旋钮](#8-nice-值普通任务之间的权重旋钮)
- [9. 实时调度：SCHED_FIFO、SCHED_RR 与 SCHED_DEADLINE](#9-实时调度sched_fifosched_rr-与-sched_deadline)
- [10. 实验一：查看与设置调度策略](#10-实验一查看与设置调度策略)
- [11. 实验二：制造并统计自愿上下文切换](#11-实验二制造并统计自愿上下文切换)
- [12. CPU 亲和性与负载均衡](#12-cpu-亲和性与负载均衡)
- [13. 只读观察命令速查](#13-只读观察命令速查)
- [14. 踩坑清单](#14-踩坑清单)
- [15. 面试真题](#15-面试真题)
- [16. 参考资料](#16-参考资料)
- [17. 上一节 | 下一节](#17-上一节--下一节)

---

## 1. 调度器到底在解决什么问题

先用大白话把问题说清楚：一颗 CPU 核心在同一时刻只能执行一个线程，但你的机器上随时有几百上千个线程想跑。谁先跑、跑多久、谁让路——这套规则就是**进程调度**，负责执行规则的内核组件就是**调度器**。

可以把 CPU 想象成一个只有一个窗口的银行大厅：

- 排队的客户就是**就绪（runnable）线程**：万事俱备，只差 CPU；
- 正在办业务的是**运行（running）线程**；
- 去补材料的人是**阻塞（blocked/sleeping）线程**：在等磁盘、等网络、等锁，就算 CPU 空着也轮不到它；
- 大堂经理就是调度器，它决定下一个叫谁的号。

调度器要同时兼顾几个互相打架的目标：

| 目标 | 通俗解释 | 与谁冲突 |
| --- | --- | --- |
| 公平 | 每个就绪线程都能分到 CPU，不许饿死 | 与优先级冲突 |
| 响应快 | 交互式任务（键盘、音频）唤醒后尽快上 CPU | 与吞吐冲突 |
| 吞吐高 | 少切换、多干活，切换本身是纯开销 | 与响应快冲突 |
| 优先级 | 重要任务先跑，实时任务有硬性期限 | 与公平冲突 |

面试里很多问题的本质都是这几个目标的取舍：为什么批处理任务适合 SCHED_BATCH？为什么实时策略没有时间片？为什么切换太频繁反而慢？想清楚"取舍"两个字，答案就顺了。

还有一个容易混淆的点先钉死：**Linux 调度的基本单位是线程（任务，task），不是进程**。同一个进程里的两个线程各有各的调度策略、优先级和 nice 值。后文说"任务"时指的就是一个可调度实体。

## 2. 什么时候会发生调度

调度不是随时随机发生的，触发点可以归成三类：

1. **主动让出**：任务自己调用了会阻塞的操作——等 IO、等锁、`sleep`、等条件变量。它从运行态变成睡眠态，内核调用 `schedule()` 挑下一个任务。
2. **被动抢占**：任务还想跑，但内核认为该换人了。典型时机是时钟中断（tick）发现它跑得太久，或者一个更高优先级的任务被唤醒。内核给它打上 `need_resched` 标记，在最近的抢占点强制切换。
3. **任务退出或让位**：任务结束，或显式调用 `sched_yield()` 让出 CPU。

这里有个高频追问：**用户态代码正在跑的时候能被抢占吗？** 能。时钟中断会周期性打进内核，中断返回用户态之前内核会检查 `need_resched`，需要的话就地切换，这就是用户态抢占。内核态代码能否被抢占则取决于内核抢占模型配置（`CONFIG_PREEMPT` 系列选项），服务器发行版和实时内核的选择不同。

Linux 的调度整体上都是**抢占式**的：只要有更高静态优先级的任务变为就绪，当前任务就会被压下去，回到它那个优先级队列里等着。

## 3. 上下文切换：切了什么，贵在哪里

上下文切换（context switch）就是"换人上 CPU"这个动作本身。要换人，得先把上一个人的现场保存好，再把下一个人的现场恢复出来。具体保存/恢复的东西包括：

- **寄存器现场**：通用寄存器、程序计数器、栈指针、状态寄存器等，存进任务的内核栈和 `task_struct` 相关结构里；
- **内核栈**：每个任务有自己的内核栈，切换任务就切换栈；
- **地址空间**：如果下一个任务属于另一个进程，还要切换页表基址（x86 上是 CR3 寄存器），这一步是进程切换和线程切换的分水岭。

由此得出一个必考结论：

| 对比项 | 同进程线程切换 | 跨进程切换 |
| --- | --- | --- |
| 寄存器/内核栈 | 要切 | 要切 |
| 页表基址（地址空间） | 不切 | 要切 |
| TLB 影响 | 小 | 大（地址空间变了，旧翻译大多作废） |
| 相对开销 | 较低 | 较高 |

注意"线程切换更便宜"不等于"免费"。切换的代价分两层：

1. **直接开销**：保存恢复寄存器、跑一遍调度器挑任务、切页表，通常是微秒量级；
2. **间接开销**：新任务把 CPU 缓存（L1/L2）和 TLB 里捂热的数据挤掉了，切回来之后一段时间内缓存命中率下降，这部分往往比直接开销更大，而且看不见、难测量。

所以性能优化的方向不是"消灭切换"（IO 密集程序天然大量切换），而是**减少无意义的切换**：锁粒度太细导致频繁睡醒、线程开太多互相挤、自旋与睡眠策略选错，都会把 CPU 时间浪费在换人和重新捂缓存上。

## 4. 自愿切换与非自愿切换

内核把上下文切换分成两类，这个分类是排障时的重要线索：

| 类型 | 发生原因 | 典型场景 |
| --- | --- | --- |
| 自愿（voluntary） | 任务自己阻塞，主动放弃 CPU | 等磁盘/网络 IO、等锁、sleep |
| 非自愿（nonvoluntary） | 任务还想跑，被内核强行抢走 | 时间片耗尽、更高优先级任务抢占 |

每个任务的累计次数记录在 `/proc/<pid>/status` 里（Linux 2.6.23 起）：

```bash
# 只读：查看当前 shell 的两类切换计数
grep ctxt_switches /proc/$$/status
# voluntary_ctxt_switches:     150
# nonvoluntary_ctxt_switches:  545
```

怎么读这两个数字：

- **自愿切换高**：说明任务频繁主动等待，多半是 IO 密集或锁等待多。是不是问题要看业务：网络服务本来就该大量等 IO；但如果一个计算型线程自愿切换奇高，就要怀疑锁竞争。
- **非自愿切换高**：说明 CPU 竞争激烈，任务经常被人抢下去。配合 `vmstat 1` 的 `cs` 列（全系统每秒切换次数）和 `r` 列（就绪队列长度）看，`r` 长期大于核数就是 CPU 不够分了。

```bash
# 只读：按进程观察每秒切换（需要安装 sysstat）
pidstat -w -p <PID> 1
# cswch/s   = 每秒自愿切换
# nvcswch/s = 每秒非自愿切换
```

记住一句排障口诀：**自愿高等待，非自愿抢 CPU；单看绝对值没意义，要结合业务类型和趋势**。

## 5. Linux 调度类：先分层，再谈公平

很多人一上来就背 CFS，其实 Linux 调度器的第一层设计是**调度类（scheduling class）**：把不同性质的任务分给不同的调度模块，模块之间有严格的优先级顺序。内核按从高到低挨个问"你那儿有没有就绪任务"，有就选它：

```
stop 类 > deadline 类 > real-time 类 > fair 类(CFS/EEVDF) > idle 类
（内核内部专用）  SCHED_DEADLINE   SCHED_FIFO/RR    SCHED_OTHER/BATCH/IDLE
```

这个分层解释了一个重要事实：**只要系统里有一个就绪的实时任务，普通任务就完全没机会跑**，不管普通任务的 nice 调成多少。公平只在同一层内部谈。

用户可见的调度策略与调度类的对应关系：

| 策略 | 所属层 | 静态优先级 | 适用场景 |
| --- | --- | --- | --- |
| SCHED_DEADLINE | deadline | 不用（用三参数） | 有明确期限的周期任务 |
| SCHED_FIFO | real-time | 1~99 | 跑到阻塞为止的关键任务 |
| SCHED_RR | real-time | 1~99 | 同优先级实时任务轮转 |
| SCHED_OTHER（内核里叫 SCHED_NORMAL） | fair | 必须为 0 | 绝大多数普通任务 |
| SCHED_BATCH | fair | 必须为 0 | CPU 密集批处理，牺牲交互换缓存 |
| SCHED_IDLE | fair | 必须为 0 | 极低优先级，比 nice 19 还弱 |

调度类在代码上是一组钩子函数（`enqueue_task`、`dequeue_task`、`pick_next_task`、`task_tick`、`wakeup_preempt` 等），调度核心只按顺序调用钩子，不关心各模块内部实现——这是教科书级的"策略与机制分离"，面试聊设计时值得提一句。

## 6. CFS：虚拟运行时间与红黑树

CFS（Completely Fair Scheduler，完全公平调度器）2.6.23 引入，长期是普通任务的默认调度器。它的核心想法一句话就能说完：**模拟一台"理想多任务 CPU"**。

理想 CPU 能同时并行跑所有就绪任务，n 个任务每个都以 1/n 的速度前进，任意时刻大家分到的 CPU 时间完全相等。真实 CPU 一次只能跑一个，怎么办？给每个任务记一个账本：

- **虚拟运行时间 vruntime**（`p->se.vruntime`，纳秒单位）：任务实际跑的物理时间，按权重折算后累加。跑得越多，vruntime 涨得越快；权重越高（nice 越低），同样物理时间折算出的 vruntime 涨得越慢。
- 调度器的选择逻辑简单到极致：**永远挑 vruntime 最小的任务跑**——也就是"到目前为止吃亏最多的那个"。

数据结构用一棵**按 vruntime 排序的红黑树**：

- 所有就绪任务挂在树上，最左节点就是 vruntime 最小者，选取是 O(1)（有缓存），插入删除 O(log n)；
- 任务跑了一会儿，vruntime 增加，被重新插回树里更靠右的位置；
- 每个运行队列维护单调递增的 `min_vruntime`，新唤醒的任务参照它放到树左侧附近，保证既不饿死也不让睡醒的任务无限插队。

为什么用红黑树而不是老式"时间片数组"？老调度器要给每个任务算固定时间片、维护活跃/过期两个数组、用一堆启发式判断谁是交互任务，容易被特定负载骗过。CFS 干脆取消固定时间片概念，用纳秒级记账 + 一棵树，没有启发式，也就没有可被攻击的启发式漏洞。

抢占时机：当前任务的 vruntime 超过最左任务一定余量（granularity，避免切得太碎毁缓存）时，就触发抢占换最左节点上。这个余量的基准可通过 `/sys/kernel/debug/sched/base_slice_ns` 调节，往小调偏低延迟桌面，往大调偏吞吐服务器。

nice 在 CFS 里的作用方式也变了：不再是"给时间片加减几毫秒"，而是**换算成权重**。当前实现中相邻 nice 级之间约差 1.25 倍权重，效果是每差一级 nice，CPU 占比大约差 10%。所以 nice 19 的任务在有竞争时真的只能吃到一点点 CPU，而 nice -20 能拿到大头。

## 7. EEVDF：内核 6.6 之后的新答案

面试答 CFS 之前先看一眼内核版本：**从 6.6 起，内核开始用 EEVDF 替换 CFS 的挑选逻辑**。EEVDF（Earliest Eligible Virtual Deadline First，最早合格虚拟截止期优先）源自 1995 年的论文，2023 年由 Peter Zijlstra 改进后合入。

它仍然追求公平，仍然记虚拟运行时间，但多了一步：

1. 用**lag（欠账值）**衡量任务是否拿到了应得的份额：lag ≥ 0 说明系统还欠它 CPU 时间（有资格，eligible），lag < 0 说明它已经超额消费；
2. 只在有资格的任务里，给每个任务算一个**虚拟截止期（virtual deadline）**，挑截止期最早的跑。

好处是**延迟敏感任务可以声明更短的时间片**：片越短虚拟截止期越早，就越容易被优先选中，交互响应更好——这是 CFS 一直靠启发式凑、EEVDF 用数学直接给的。任务还能通过 `sched_setattr()` 显式请求时间片长度。

对"睡一觉刷新账本"的作弊，EEVDF 用基于虚拟时间的 lag 衰减机制处理：短暂睡眠不让负 lag 立刻清零，长睡才逐渐重置。

面试怎么答最稳："普通任务的调度长期由 CFS 承担，核心是 vruntime + 红黑树；6.6 起逐步换成 EEVDF，在公平的基础上用虚拟截止期显式照顾延迟敏感任务。"既展示深度，又不会把老实现说成现状。

## 8. nice 值：普通任务之间的权重旋钮

nice 是普通任务（SCHED_OTHER / SCHED_BATCH）之间分配 CPU 的旋钮：

- 范围 **-20 ~ +19**，**数字越小优先级越高**（对别人越"不 nice"）；
- 默认 0；调高 nice（变谦让）任何用户都可以，调低 nice（抢更多 CPU）需要权限（CAP_SYS_NICE 或 RLIMIT_NICE 允许）；
- POSIX 说 nice 是进程属性，**Linux 实现是线程属性**，同进程不同线程可以不同 nice；
- nice **只影响 fair 类**，对 SCHED_FIFO/RR 完全无效；SCHED_IDLE 下 nice 也无效（它本来就低到地板了）。

常用命令：

```bash
# 以 nice +10 启动一个编译任务（谦让，不需要特权）
nice -n 10 make -j8

# 慎用：把已运行进程的 nice 调到 -5（抢更多 CPU，需要特权）
sudo renice -n -5 -p <PID>

# 只读：查看进程 nice（top 的 NI 列，或 ps）
ps -o pid,ni,comm -p <PID>
```

两个关键理解：

1. **nice 是比例不是上限**：nice 19 的任务在系统空闲时照样能吃满 CPU，它只在有竞争时被压到很小的份额。想硬性限制 CPU 用量，用 cgroup 的 cpu 控制器（带宽上限），不是 nice。
2. **组调度会稀释 nice 的效果**：开启 CONFIG_FAIR_GROUP_SCHED 后，公平先在组之间分（cgroup 的 cpu.shares/cpu.weight），再在组内分。桌面系统的 autogroup 特性按会话分组，所以你在一个终端里 renice 单个进程，可能感觉"没什么用"——它先要跟整个会话组竞争。

## 9. 实时调度：SCHED_FIFO、SCHED_RR 与 SCHED_DEADLINE

实时（real-time）的含义不是"快"，而是**可预期的确定性**：关键时刻我必须立刻拿到 CPU。实时任务静态优先级 1~99，**数字越大优先级越高**（注意和 nice 方向相反！），且永远压过所有普通任务。

### SCHED_FIFO：跑到自己停

没有时间片。一个 FIFO 任务上 CPU 后一直跑，直到三种情况之一：自己阻塞（IO/锁/sleep）、被更高优先级实时任务抢占、主动 `sched_yield()`。同优先级任务严格按先来后到：新就绪的排到同优先级队尾，被抢占后恢复的仍留在队头。

### SCHED_RR：同优先级轮转

在 FIFO 规则上加一条：每个任务最多连跑一个**时间片**（quantum），用完排到同优先级队尾，让同优先级的兄弟跑。时间片长度用 `sched_rr_get_interval()` 查询。被高优先级抢占再恢复时，继续用完剩余时间片。**RR 的轮转只发生在同优先级之间**，对不同优先级没有任何影响。

### SCHED_DEADLINE：带期限的任务

Linux 3.14 引入，用户可控的最高优先级。用 `sched_setattr()` 声明三个纳秒参数：

- **Runtime**：每个周期最多需要多少 CPU 时间；
- **Deadline**：任务激活后多久之内必须完成；
- **Period**：多久激活一次。

内核要求 `Runtime ≤ Deadline ≤ Period`，并做**准入测试**：所有 deadline 任务的利用率总和（Σ Runtime/Period）不能超过可用 CPU 数，超了直接返回 EBUSY 拒绝设置。运行中超出 Runtime 会被 CBS（恒定带宽服务器）机制节流，保证任务之间互不干扰。实现上是 GEDF（全局最早截止期优先）+ CBS。

### 三者速查

| 策略 | 时间片 | 同级排队规则 | 典型用途 |
| --- | --- | --- | --- |
| SCHED_FIFO | 无 | 先来后到，跑完为止 | 低延迟采集、控制回路 |
| SCHED_RR | 有（可查询） | 同级轮转 | 多个同重要性实时线程 |
| SCHED_DEADLINE | Runtime 限额 | 截止期最早优先 | 周期性、有硬期限的任务 |

### 权限与安全阀

设置实时策略需要 CAP_SYS_NICE，或者 RLIMIT_RTPRIO 软限非零（普通用户只能设到不超过该上限的实时优先级）。另外两个保命机制值得知道：

- **RT 限流**：默认内核只允许实时任务占用每个周期约 95% 的 CPU（`sched_rt_runtime_us`/`sched_rt_period_us`，默认 950000/1000000），留 5% 给普通任务，防止一个 FIFO 死循环把整台机器焊死；
- **reset-on-fork**：设置 SCHED_RESET_ON_FORK 标志后，fork 出的子进程自动降回 SCHED_OTHER、负 nice 归零，防止实时程序靠 fork 绕过 RLIMIT_RTTIME 限制。SCHED_DEADLINE 任务默认 fork 直接失败（EAGAIN），必须先设这个标志。

## 10. 实验一：查看与设置调度策略

下面这段代码查询当前线程的策略与优先级，演示 nice 调整和实时策略的权限报错。保存为 `sched_info.c`：

```c
// sched_info.c —— 查看/设置调度策略与优先级
// 编译：gcc -Wall -o sched_info sched_info.c
#define _GNU_SOURCE
#include <stdio.h>
#include <sched.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>

// 把策略编号翻译成人类可读的名字
static const char *policy_name(int p)
{
    switch (p) {
    case SCHED_OTHER: return "SCHED_OTHER（普通分时）";
    case SCHED_FIFO:  return "SCHED_FIFO（实时先进先出）";
    case SCHED_RR:    return "SCHED_RR（实时轮转）";
    case SCHED_BATCH: return "SCHED_BATCH（批处理）";
    case SCHED_IDLE:  return "SCHED_IDLE（极低优先级）";
#ifdef SCHED_DEADLINE
    case SCHED_DEADLINE: return "SCHED_DEADLINE（截止期）";
#endif
    default: return "未知策略";
    }
}

int main(void)
{
    // sched_* 接口里 pid 传 0 表示"调用线程自己"
    int policy = sched_getscheduler(0);
    struct sched_param sp;
    sched_getparam(0, &sp);              // 取静态优先级
    printf("当前策略：%s\n", policy_name(policy));
    printf("静态优先级 sched_priority：%d（普通策略恒为 0）\n",
           sp.sched_priority);

    // 实时优先级范围用接口查，不要硬编码 1~99：
    // POSIX 只要求至少 32 级，可移植程序必须查询
    printf("SCHED_FIFO 优先级范围：%d ~ %d\n",
           sched_get_priority_min(SCHED_FIFO),
           sched_get_priority_max(SCHED_FIFO));

    // getpriority 可能合法地返回 -1（nice 就是 -1），
    // 所以先清 errno，返回后靠 errno 判断是否真出错
    errno = 0;
    int old_nice = getpriority(PRIO_PROCESS, 0);
    if (old_nice == -1 && errno != 0) {
        perror("getpriority");
        return 1;
    }
    // 调高 nice（谦让）不需要任何特权
    if (setpriority(PRIO_PROCESS, 0, old_nice + 5) == -1) {
        perror("setpriority");
    } else {
        printf("nice：%d -> %d（数字越大越谦让）\n",
               old_nice, old_nice + 5);
    }

    // 尝试把自己升为实时任务：普通用户通常没权限，
    // 这里演示正确的失败处理，而不是当成程序 bug
    struct sched_param rt = { .sched_priority = 10 };
    if (sched_setscheduler(0, SCHED_FIFO, &rt) == -1) {
        printf("设置 SCHED_FIFO 失败：%s\n", strerror(errno));
        puts("属正常现象：需要 CAP_SYS_NICE 或非零 RLIMIT_RTPRIO");
    } else {
        puts("已切换为 SCHED_FIFO（说明你有实时权限，慎用）");
    }
    return 0;
}
```

运行方式：

```bash
gcc -Wall -o sched_info sched_info.c
./sched_info          # 普通用户：实时设置那步预期报 EPERM
sudo ./sched_info     # 有特权时能真正切到 SCHED_FIFO
```

预期现象：普通运行下策略是 SCHED_OTHER、静态优先级 0、nice 成功 +5、设置实时失败并打印"Operation not permitted"。这个"失败"本身就是知识点：**EPERM 是权限模型在正常工作，不是代码写错了**。

## 11. 实验二：制造并统计自愿上下文切换

两个线程用管道互传一个字节：每次 `read` 没数据就阻塞，主动让出 CPU，形成一次自愿切换。跑固定轮数，对比 `/proc/self/status` 里的计数增量。保存为 `ctx_pingpong.c`：

```c
// ctx_pingpong.c —— 用管道乒乓制造自愿上下文切换并计数
// 编译：gcc -Wall -o ctx_pingpong ctx_pingpong.c -pthread
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define ROUNDS 50000   // 乒乓轮数

static int ab[2];      // 管道：主线程 -> 子线程
static int ba[2];      // 管道：子线程 -> 主线程

// 从 /proc/self/status 读本进程累计自愿切换次数
static long read_voluntary_ctx(void)
{
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) { perror("fopen"); exit(1); }
    char line[256];
    long val = -1;
    while (fgets(line, sizeof(line), fp)) {
        // 匹配到目标行就 sscanf 取出数字
        if (sscanf(line, "voluntary_ctxt_switches: %ld", &val) == 1)
            break;
    }
    fclose(fp);
    return val;
}

// 阻塞读 1 字节，读不到 1 字节就报错退出（管道关闭等异常）
static void must_read(int fd, char *c)
{
    ssize_t n = read(fd, c, 1);
    if (n != 1) { perror("read"); exit(1); }
}

// 写 1 字节，写失败就报错退出
static void must_write(int fd, const char *c)
{
    if (write(fd, c, 1) != 1) { perror("write"); exit(1); }
}

static void *pong(void *arg)
{
    (void)arg;
    char c;
    for (int i = 0; i < ROUNDS; i++) {
        must_read(ab[0], &c);   // 没数据 -> 阻塞 -> 自愿让出 CPU
        must_write(ba[1], &c);  // 回传，唤醒主线程
    }
    return NULL;
}

int main(void)
{
    // 建两条单向管道，组成乒乓回路
    if (pipe(ab) == -1 || pipe(ba) == -1) { perror("pipe"); return 1; }

    long before = read_voluntary_ctx();

    pthread_t tid;
    if (pthread_create(&tid, NULL, pong, NULL) != 0) {
        fprintf(stderr, "pthread_create 失败\n");
        return 1;
    }

    char c = 'x';
    for (int i = 0; i < ROUNDS; i++) {
        must_write(ab[1], &c);  // 发球，唤醒子线程
        must_read(ba[0], &c);   // 等回球 -> 阻塞 -> 自愿切换
    }
    pthread_join(tid, NULL);

    long after = read_voluntary_ctx();
    printf("轮数：%d\n", ROUNDS);
    printf("voluntary_ctxt_switches：%ld -> %ld（增量 %ld）\n",
           before, after, after - before);
    puts("增量不保证恰好等于 2*轮数：read 时数据已就绪就不会阻塞，"
         "进程里其他事件也会计入");
    return 0;
}
```

运行方式与观察点：

```bash
gcc -Wall -o ctx_pingpong ctx_pingpong.c -pthread
./ctx_pingpong
```

预期现象：增量与 2×ROUNDS 同一量级（数万到十万次）。这个实验把三件事连起来了：**阻塞 = 主动让出 CPU = 自愿切换**；管道乒乓是典型的"切换密集型"负载，每传一个字节都要睡醒两次，这就是为什么高频小消息场景要攒批（batching）或用无锁队列减少睡醒。注意计数器是**进程内所有线程**的累计值，且增量不保证精确等于理论次数。

## 12. CPU 亲和性与负载均衡

多核机器上调度器还有一份工作：把任务摊到各个核上（负载均衡），避免一个核挤死、其他核围观。但有时你反而想**钉住**任务：

- **缓存 locality**：任务固定在一个核上跑，L1/L2 缓存能持续复用，迁移一次缓存就白捂了；
- **NUMA**：跨节点访问内存更慢，把线程和它的内存钉在同一节点；
- **隔离干扰**：实时线程独占一个核，普通任务赶到其他核（配合内核参数 isolcpus / cpuset）。

接口是 `sched_setaffinity()` / `sched_getaffinity()`，命令行是 `taskset`：

```bash
# 只读：查看进程当前绑在哪些核
taskset -pc <PID>

# 慎用：把进程钉到 2、3 号核（改变调度行为，可能加剧不均）
taskset -pc 2,3 <PID>

# 启动时就绑核
 taskset -c 0 ./my_realtime_app
```

代价也要说清楚：绑核放弃了调度器的负载均衡，绑的核忙死、别的核闲着也没人救你。所以绑核是**测量之后的定向优化**，不是默认动作。

## 13. 只读观察命令速查

全部只读，随便贴：

```bash
# 当前 shell 的调度策略与实时优先级
chrt -p $$

# 进程 nice 与优先级（NI 列是 nice，PRI 列是内核优先级）
ps -o pid,ni,pri,cls,comm -p <PID>

# 两类上下文切换计数
grep ctxt_switches /proc/<PID>/status

# 全系统每秒切换次数（cs）与就绪队列长度（r）
vmstat 1

# 按进程的每秒自愿/非自愿切换（sysstat 包）
pidstat -w -p <PID> 1

# 每个核的利用率，看负载是否偏科
top     # 按 1 展开每核
```

慎用类（会改变系统调度状态，放到最后单独列出）：

```bash
# 以 SCHED_FIFO 优先级 10 运行程序（需要特权；程序失控可能长时间霸占 CPU）
sudo chrt -f 10 ./my_app

# 调整已运行线程为 SCHED_RR（注意 chrt 操作的是线程）
sudo chrt -r -p 20 <TID>
```

## 14. 踩坑清单

1. **nice 和实时优先级方向相反**：nice 越小越优先（-20 最强），实时优先级越大越优先（99 最强）。面试紧张时说反是高频翻车点。
2. **Linux 的 nice 是线程属性**，不是 POSIX 说的进程属性；同进程两个线程可以不同 nice。
3. **getpriority 返回 -1 不一定是错**：nice 本来就可能是 -1，必须先清 errno 再判断。
4. **普通策略的 sched_priority 必须为 0**：给 SCHED_OTHER 传非零静态优先级会 EINVAL，普通任务的"优先级"由 nice 表达。
5. **设置实时策略报 EPERM 不是 bug**：需要 CAP_SYS_NICE 或非零 RLIMIT_RTPRIO，代码要正确处理这个失败。
6. **SCHED_FIFO 死循环可能长时间霸占 CPU**：默认 RT 限流留了约 5% 给普通任务保命，但机器仍会卡到几乎不可用；写实时代码必须保证有阻塞点或主动让出。
7. **sched_yield 不是万能让步**：在 CFS 下它效果有限，在 FIFO 下会排到同优先级队尾、在 DEADLINE 下要等下个周期；拿它做自旋等待通常是反模式。
8. **别硬编码实时优先级 1~99**：POSIX 只保证至少 32 级，可移植代码用 sched_get_priority_min/max 查询。
9. **SCHED_IDLE 下 nice 无效**：它已经低于 nice 19，别再试图用 renice 调它。
10. **上下文切换的开销大头常在间接成本**：缓存与 TLB 变冷带来的损失比保存寄存器本身更大，"线程切换便宜"不等于免费。
11. **cs 高不等于有故障**：IO 密集型服务自愿切换天然高；要结合 r 队列长度、CPU 利用率和业务类型判断。
12. **nice 19 不等于"最多用一点 CPU"**：系统空闲时它照样吃满；nice 是竞争时的比例，不是硬上限，硬限制用 cgroup 带宽。
13. **chrt 改的是线程不是整个进程**：多线程程序要逐线程设置，或在创建线程时用 pthread_attr_setschedpolicy 指定。
14. **fork 会继承调度策略和优先级**：实时进程 fork 出的孩子也是实时的，可能意外扩散；用 SCHED_RESET_ON_FORK 防止。SCHED_DEADLINE 不设该标志 fork 直接 EAGAIN。
15. **背 CFS 前先说内核版本**：6.6+ 已在向 EEVDF 迁移，把 vruntime/红黑树说成"当前最新实现"会显得知识过期；答"CFS 长期实现 + EEVDF 演进"最稳。

## 15. 面试真题

**1. 什么是上下文切换？都切换了什么？**

上下文切换是内核把 CPU 从一个任务换给另一个任务的过程，需要保存当前任务的寄存器、程序计数器和内核栈现场，再恢复下一个任务的现场。跨进程切换还要切换页表基址（地址空间），同进程线程切换则不用。开销除了保存恢复本身，更大的部分往往是缓存和 TLB 变冷造成的间接损失。

**2. 进程切换和线程切换哪个贵？为什么？**

跨进程切换更贵，因为多了地址空间切换这一步，页表基址一换，TLB 里旧的地址翻译大多失效。同进程线程共享地址空间，跳过这一步，TLB 影响小。但两者都要保存恢复寄存器、都要跑调度器，线程切换并不免费。

**3. 自愿和非自愿上下文切换的区别？怎么观察？**

自愿切换是任务自己阻塞（等 IO、等锁）主动让出 CPU；非自愿切换是任务还想跑但被抢占（时间片到、更高优先级就绪）。看 `/proc/<pid>/status` 的两个 ctxt_switches 计数，或 pidstat -w 的每秒速率。自愿高查等待和锁竞争，非自愿高查 CPU 争抢，结合 vmstat 的 r 和 cs 判断。

**4. CFS 的核心思想是什么？**

CFS 模拟一台理想多任务 CPU：n 个就绪任务每个都以 1/n 速度并行前进。真实 CPU 做不到并行，就给每个任务记按权重折算的虚拟运行时间 vruntime，永远挑 vruntime 最小（吃亏最多）的任务跑。跑得多的 vruntime 涨得快自然靠后，公平就这样涌现出来，不需要固定时间片和交互性启发式。

**5. CFS 为什么用红黑树？**

就绪任务按 vruntime 排序挂在红黑树上，最左节点就是最该跑的任务，选取 O(1)（有缓存）、插入删除 O(log n)，复杂度稳定。相比老调度器的活跃/过期双数组，没有数组整体切换的抖动，也没有可被特定负载欺骗的启发式。每个运行队列再维护单调的 min_vruntime，给新唤醒任务一个合理的落点。

**6. nice 值怎么起作用？对实时线程有用吗？**

nice 范围 -20~19，数字越小优先级越高，在 CFS 里换算成权重，相邻级约差 1.25 倍，每级约影响 10% 的 CPU 占比。它只影响普通任务（SCHED_OTHER/BATCH），对 SCHED_FIFO/RR 完全无效，SCHED_IDLE 下也无效。另外 Linux 的 nice 是线程属性，且只是竞争比例不是用量上限。

**7. SCHED_FIFO 和 SCHED_RR 的区别？**

两者都是实时策略，优先级 1~99，永远压过普通任务。FIFO 没有时间片，一直跑到自己阻塞、被更高优先级抢占或主动 yield；RR 加了时间片，用完排到同优先级队尾轮转。RR 的轮转只发生在同优先级之间，对不同优先级没有任何影响。

**8. 实时调度有什么风险？内核怎么兜底？**

一个 SCHED_FIFO 死循环能饿死同核所有普通任务，机器近乎失联。内核默认 RT 限流把实时任务限制在约 95% 带宽内，留 5% 给普通任务保命。还有 RLIMIT_RTPRIO 限制普通用户能设的实时优先级、reset-on-fork 防止实时属性经 fork 扩散，生产上再用 cpuset 隔离实时核。

**9. SCHED_DEADLINE 是什么？三个参数什么关系？**

它是用户可控最高优先级的截止期调度策略，声明 Runtime（每周期最多跑多久）、Deadline（激活后多久内完成）、Period（激活周期），要求 Runtime ≤ Deadline ≤ Period。内核做准入测试，总利用率超过 CPU 数就拒绝（EBUSY），运行时超额会被 CBS 节流。实现是 GEDF + CBS，适合有硬期限的周期任务。

**10. EEVDF 相比 CFS 改进了什么？**

EEVDF（6.6 起逐步替换 CFS）先用 lag 判断任务是否有资格（系统是否还欠它 CPU），再在合格任务里挑虚拟截止期最早的跑。延迟敏感任务可以申请更短时间片，截止期更早从而更快被调度，把 CFS 靠启发式凑的交互优化变成显式机制。它还用 lag 衰减防止短暂睡眠刷账本。

**11. 调度类和优先级是什么关系？**

Linux 先按调度类分层：stop > deadline > 实时 > fair（CFS/EEVDF）> idle，高层有就绪任务时低层完全没机会。优先级数字只在层内起作用：实时层比 1~99，fair 层比 nice 权重。所以"nice -20 也抢不过优先级 1 的实时线程"，公平只在同层内谈。

**12. 抢占发生在什么时机？用户态代码能被抢占吗？**

三类时机：任务主动阻塞触发 schedule()；时钟中断或高优先级唤醒打上 need_resched 标记，在抢占点被动切换；任务退出或 yield。用户态代码可以被抢占——中断返回用户态前内核检查标记就地切换。内核态能否抢占取决于 CONFIG_PREEMPT 配置。

**13. CPU 亲和性有什么用？什么时候不该用？**

绑核让任务固定在指定 CPU 上，复用该核的热缓存、避免迁移，NUMA 场景还能保证本地内存访问，实时场景配合核隔离排除干扰。接口是 sched_setaffinity，命令行是 taskset。但它放弃了负载均衡，绑的核过载别的核也帮不上，所以应该是测量后的定向优化，不是默认操作。

## 16. 参考资料

本章知识点覆盖对照了以下公开资料，正文与示例代码为作者原创重写，未整段复制原文：

- Linux 手册 sched(7)：调度策略、nice、实时规则与权限模型 —— https://man7.org/linux/man-pages/man7/sched.7.html
- Linux 内核文档 CFS Scheduler：vruntime、红黑树、调度类与组调度 —— https://docs.kernel.org/scheduler/sched-design-CFS.html
- Linux 内核文档 EEVDF Scheduler：lag、虚拟截止期与 6.6 迁移 —— https://docs.kernel.org/scheduler/sched-eevdf.html
- Linux 手册 proc_pid_status(5)：voluntary/nonvoluntary_ctxt_switches 字段 —— https://man7.org/linux/man-pages/man5/proc_pid_status.5.html

## 17. 上一节 | 下一节

- 上一节：[5.1 虚拟内存与页表](../01-virtual-memory/README.md)
- 下一节：5.3 内存管理与 OOM（待写）

> 作者：青松与桑叶。理解调度的关键，不是背下 CFS 的每个细节，而是能说清"谁在什么时候、凭什么拿到 CPU，换人时付出了什么代价"。把分层、公平、优先级、切换成本这四件事串起来，调度题就通了。
