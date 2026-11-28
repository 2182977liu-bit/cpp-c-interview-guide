# 5.3 内存管理与 OOM

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

## 目录

- [1. 内存管理到底在管什么](#1-内存管理到底在管什么)
- [2. 伙伴系统：按 2 的幂管理物理页](#2-伙伴系统按-2-的幂管理物理页)
- [3. 内存碎片与内存规整](#3-内存碎片与内存规整)
- [4. slab 分配器：内核的小对象仓库](#4-slab-分配器内核的小对象仓库)
- [5. 页缓存与匿名页：两种完全不同的内存](#5-页缓存与匿名页两种完全不同的内存)
- [6. 内存回收：水位线与 kswapd](#6-内存回收水位线与-kswapd)
- [7. swap：匿名页的退路](#7-swap匿名页的退路)
- [8. overcommit：malloc 成功不等于有内存](#8-overcommitmalloc-成功不等于有内存)
- [9. 读懂 /proc/meminfo](#9-读懂-procmeminfo)
- [10. OOM killer：最后的选择与牺牲者](#10-oom-killer最后的选择与牺牲者)
- [11. 只读观察命令速查](#11-只读观察命令速查)
- [12. 实验一：观察 malloc 与真实物理内存](#12-实验一观察-malloc-与真实物理内存)
- [13. 实验二：查看并调整自己的 OOM 分数](#13-实验二查看并调整自己的-oom-分数)
- [14. 踩坑清单](#14-踩坑清单)
- [15. 面试真题](#15-面试真题)
- [16. 参考资料](#16-参考资料)
- [17. 上一节 | 下一节](#17-上一节--下一节)

---

## 1. 内存管理到底在管什么

上一章讲了 CPU 怎么分（调度），这一章讲内存怎么分。面试里"伙伴系统、slab、swap、OOM"这四个词经常一起出现，很多人把它们当成四个孤立的知识点背，其实它们是**同一条供给链上的四个环节**。

先用一个仓库的比喻把整条链串起来：

- **伙伴系统**是"土地管理局"：物理内存被切成一页一页（通常 4KB），它负责整块整块地批地、收地，最小单位就是页。
- **slab 分配器**是"精装房中介"：内核自己要用的对象（进程描述符、inode、dentry 等）大多远小于一页，直接批一页太浪费，slab 从伙伴系统拿到页后切成小格子反复出租。
- **页缓存和匿名页**是住进房子的两类"住户"：一类存的是文件内容（磁盘上有备份），一类存的是进程的堆栈数据（磁盘上没备份）。
- **回收与 swap** 是"腾退机制"：内存紧张时，有备份的住户直接请走（需要时再从磁盘读回来），没备份的住户先搬到 swap 这个"临时仓库"再请走。
- **OOM killer** 是"最后的强制手段"：腾退也腾不出来时，内核挑一个进程直接杀掉，保住整个系统。

分层速查表：

| 层次 | 管理对象 | 粒度 | 谁在用 |
|------|----------|------|--------|
| 伙伴系统（页分配器） | 物理页帧 | 2^order 个连续页 | 内核所有需要页的地方 |
| slab 分配器 | 内核小对象 | 几字节到几 KB | kmalloc、task_struct、inode 等 |
| 用户态分配器（glibc malloc） | 进程堆内存 | 任意字节数 | 应用程序（详见 1.3 节） |

注意一个容易混淆的点：**malloc 不是内核接口**。glibc 的 malloc 在用户态管理堆，向内核要内存时走 brk/mmap 系统调用，内核再经过缺页中断从伙伴系统拿物理页。所以"malloc 返回成功"和"物理页到手"是两件事，第 8 节会专门讲。

## 2. 伙伴系统：按 2 的幂管理物理页

### 2.1 基本规则

伙伴系统（buddy system）把空闲物理页按 **2 的幂** 组织成 11 个链表：order 0 是 1 页（4KB），order 1 是 2 页，order 2 是 4 页……order 10 是 1024 页（4MB）。这也是一次性能从伙伴系统拿到的最大连续块（默认 MAX_ORDER 配置下）。

分配和释放的规则用两句话就能说清：

1. **分配要劈**：要 1 页但 order 0 没了？从 order 1 拿一块劈成两半，一半给你，另一半挂回 order 0 链表。order 1 也没有就继续往上找，一路劈下来。
2. **释放要合**：释放一块时，检查它的"伙伴"（同 order、物理地址相邻、且按该 order 对齐的那一块）是否也空闲，是就合并成更大的块挂到上一级，能合就一直合。

"伙伴"的定义是面试细节题：**两块大小相同、物理连续、合并后地址仍按大块对齐**，才互为伙伴。不是随便两块相邻空闲页都能合。

### 2.2 观察：/proc/buddyinfo

```bash
# 只读：每个 zone 一行，从左到右是 order 0~10 的空闲块数量
cat /proc/buddyinfo
```

输出长这样（数字随机器变化）：

```
Node 0, zone      DMA      1      1      1      0      2 ...
Node 0, zone    Normal   1832    940    421    147     52 ...
```

解读方法：**左边列大、右边列全是 0**，说明空闲页不少但全是零散单页——这就是外部碎片严重的典型形态。此时申请 2MB 连续物理内存（比如透明大页）会失败或触发规整，尽管 `free` 显示还有很多空闲内存。

### 2.3 碎片的两张面孔

| 碎片类型 | 含义 | 伙伴系统的表现 | 对策 |
|----------|------|----------------|------|
| 外部碎片 | 空闲总量够，但不连续 | 高 order 链表为空 | 内存规整（compaction）、按迁移类型分组 |
| 内部碎片 | 分到的块比需要的大 | 要 5KB 给 8KB（order 1） | slab 负责小对象，避免整页浪费 |

伙伴系统还按**迁移类型**（不可移动/可移动/可回收）把页分组存放，尽量不让"钉死"的内核页和"能搬走"的用户页混在同一个页块里——这直接决定了后面规整能不能成功。

## 3. 内存碎片与内存规整

虚拟内存能把零散物理页拼成连续的虚拟地址（5.1 节讲过页表），所以普通应用不在乎物理连续。但有两类用户在乎：**设备 DMA 缓冲区**和**大页（THP/hugetlbfs）**，它们要的是物理连续。

内存规整（compaction）的思路很朴素：把 zone 里**可移动的页**往高处搬，把空闲页往低处聚，聚出一段连续物理区间。搬页之所以可行，是因为用户访问的是虚拟地址——物理页换了位置，改一下页表映射即可，进程毫无感知。

- 后台由 `kcompactd` 内核线程异步做；分配路径上也可能同步触发。
- `echo 1 > /proc/sys/vm/compact_memory` 可手动触发全系统规整（**慎用**：有 CPU 开销，只在排查碎片问题时用）。
- 不可移动页（内核数据结构、mlock 的页）搬不走，它们是规整失败的主要原因。

面试一句话：**回收解决"总量不够"，规整解决"不连续"，两者是正交的机制。**

## 4. slab 分配器：内核的小对象仓库

### 4.1 为什么需要它

内核每秒要创建销毁成千上万个小对象：一个 `task_struct` 几 KB，一个 dentry 一两百字节。如果每次都找伙伴系统要整页：

1. 内部碎片爆炸——192 字节的对象占 4KB 页，浪费 95%；
2. 反复劈页合页，伙伴系统开销大；
3. 对象刚释放又马上要用（比如频繁的 open/close），每次重新初始化也浪费。

slab 的方案：**为每类对象建一个专用缓存（cache）**，从伙伴系统批发若干页，切成等大的格子（object）。释放对象时格子不还给伙伴系统，留着下次直接复用，连初始化都能省一部分。

### 4.2 三种实现

| 实现 | 特点 | 现状 |
|------|------|------|
| SLAB | 最早的实现，每 CPU/每节点多级队列，元数据多 | 保留但不再是主流，较新内核已移除 |
| SLUB | 简化队列、元数据内嵌进页结构，调试特性完善 | **当前默认** |
| SLOB | 面向极小内存的嵌入式系统 | 已在较新内核中移除 |

面试答"slab 和 slub 什么区别"时，抓住"SLUB 是 SLAB 的简化重写，去掉复杂队列、降低元数据开销，接口兼容"即可，不必背实现细节。

### 4.3 通用缓存：kmalloc

内核代码里最常见的 `kmalloc(size, flags)` 并没有为每种 size 建缓存，而是使用一组**按大小分档的通用缓存**：kmalloc-8、kmalloc-16、kmalloc-32……kmalloc-8192。要 30 字节就从 kmalloc-32 拿一格——所以 kmalloc 也有自己的内部碎片（向上取整到档位）。

### 4.4 观察：/proc/slabinfo 与 slabtop

```bash
# 只读（需要 root）：每行一个缓存
sudo cat /proc/slabinfo | head -8
# 只读（需要 root）：按占用排序的实时视图
sudo slabtop -o | head -20
```

`/proc/slabinfo` 关键列：`active_objs`（在用对象数）、`num_objs`（总格子数）、`objsize`（每格字节数）、`objperslab`（每个 slab 含几格）、`pagesperslab`（每个 slab 占几页）。`num_objs - active_objs` 就是缓存着的空闲格子。

一个重要的坑：**只要一个 slab 里还有一个对象在用，整页就不能还给伙伴系统**。所以"对象大部分释放了但 Slab 占用没降"是正常现象，不一定是泄漏。

slab 总量在 `/proc/meminfo` 里分两类：`SReclaimable`（dentry/inode 这类可从磁盘重建的缓存，内存紧张时可回收）和 `SUnreclaim`（真正钉住的内核对象）。两者之和是 `Slab`。

## 5. 页缓存与匿名页：两种完全不同的内存

物理页里装的数据分两大类，回收方式完全不同，这是理解 swap 和 OOM 的前提。

| 对比项 | 文件页（page cache） | 匿名页（anonymous） |
|--------|---------------------|---------------------|
| 内容 | 文件数据的缓存 | 堆、栈、私有 mmap |
| 磁盘上有备份吗 | 有（就是原文件） | 没有 |
| 干净页怎么回收 | 直接丢弃，要用再读文件 | 不存在"干净匿名页"一说（未写过的映射只指向零页） |
| 脏页怎么回收 | 写回原文件后丢弃 | 必须先写入 swap 才能丢 |
| 没有 swap 时 | 照常可回收 | **基本不可回收** |

两个细节值得在面试里主动讲：

1. **读不分配、写才分配**：malloc 后只读不写，缺页中断只会把页表指向一个全局共享的"零页"；第一次写才真正从伙伴系统拿物理页。这就是"申请 1GB 只摸 300MB，RSS 就只有 300MB"的原理。
2. **tmpfs/shmem 是特殊的第三类**：它计入 Cached，看起来像文件页，但背后没有磁盘文件，回收时必须走 swap。没有 swap 的机器上 tmpfs 里的数据同样钉在内存里。

## 6. 内存回收：水位线与 kswapd

每个 zone 有三条水位线（watermark），由 `vm.min_free_kbytes` 等参数推导：

| 水位 | 空闲页低于它时发生什么 |
|------|------------------------|
| high | kswapd 回收到 high 以上就回去睡觉 |
| low | 唤醒 `kswapd` 内核线程，**异步**扫描并回收页面，分配请求不受影响 |
| min | 分配路径进入**直接回收（direct reclaim）**：申请者自己同步扫页腾内存，**分配被卡住**，表现为莫名的延迟毛刺 |

min 以下还留了一小口"应急粮"给关键分配路径（PF_MEMALLOC），避免回收本身需要内存时死锁——这也是 `min_free_kbytes` 不能设太小的原因（官方文档明确说低于 1MB 系统会在高负载下变得容易死锁；设太大则直接人为制造 OOM）。

回收扫描的对象按 LRU 组织，分文件页链表和匿名页链表，**swappiness 决定两边扫描的倾斜程度**（下一节）。观察手段：

```bash
# 只读：si/so 是每秒换入/换出的 KB 数，持续非 0 说明在动 swap
vmstat 1
# 只读：内存压力失速信息（PSI），some/full 的 avg10 明显大于 0 说明有任务在等内存
cat /proc/pressure/memory
```

`vmstat` 里 si/so 长期为 0 但应用还是卡顿，再看 PSI 和直接回收计数（`/proc/vmstat` 的 `allocstall_*`），能区分"在等 swap IO"还是"在等直接回收"。

## 7. swap：匿名页的退路

### 7.1 本质

swap 是磁盘（或压缩内存）上的一块空间，充当**匿名页的临时备份**。有了它，长期不用的堆内存可以换出（swap out），把物理页让给页缓存或活跃进程；再次访问时缺页中断把它换入（swap in）。`/proc/meminfo` 的 `SwapCached` 记录"换回来过但 swap 里还留着副本"的页——再回收它们时不用重新写盘，省一次 IO。

**swap 不是内存扩容**：磁盘随机 IO 比内存慢几个数量级，换入换出频繁到影响前台任务时叫"颠簸（thrashing）"，体验比直接 OOM 还差。swap 的正确用途是给"冷数据"一个去处，而不是让热数据也去磁盘上跑。

### 7.2 swappiness 的真实语义

常见误区是把它当"使用 swap 的百分比"。较新内核的定义是：**换出匿名页与回收文件页的相对 IO 成本**，取值 0~200，默认 60：

- 100：认为两者 IO 成本相等，两边平均施压；
- 调低：认为 swap 更贵，优先回收文件页（数据库类服务常这么调）；
- 调高（>100）：swap 设备比文件系统还快时才合理，典型是 zram/zswap；
- **0 不等于禁用 swap**：只是尽量不换，直到空闲页加文件页跌破 high 水位仍会换出。真要禁用请 `swapoff`（慎用）。

### 7.3 建一个 swap 文件（实验环境用，慎用）

以下步骤会改系统状态，只在自己的实验机/虚拟机上做：

```bash
sudo dd if=/dev/zero of=/swapfile bs=1M count=1024   # 建 1GB 文件
sudo chmod 600 /swapfile                             # 权限必须收紧
sudo mkswap /swapfile                                # 格式化为 swap
sudo swapon /swapfile                                # 启用
swapon --show                                        # 只读：确认生效
# 实验完撤销：sudo swapoff /swapfile && sudo rm /swapfile
```

嵌入式场景更常见的是 **zram**（内存里开压缩块设备当 swap，用 CPU 换容量）和 **zswap**（换出前先压缩缓存在内存，压不下再写盘），它们没有磁盘 IO，swappiness 反而应该调高。

## 8. overcommit：malloc 成功不等于有内存

### 8.1 为什么允许"超卖"

程序 malloc 1GB 往往只是"以防万一"，实际摸不了多少；fork 之后有写时复制，子进程也不会立刻真占一份。如果内核按申请量老老实实预留物理内存，利用率会低得可怜。所以 Linux 默认**超额承诺（overcommit）**：先答应你，真用到时再给。

代价是：兑现不了的那天，只能靠 OOM killer 收场。**OOM 不是 malloc 失败的兜底，而是 overcommit 的结算方式。**

### 8.2 三种模式

`/proc/sys/vm/overcommit_memory`：

| 模式 | 策略 | 适用 |
|------|------|------|
| 0（默认） | 启发式：明显离谱的申请（远超 RAM+swap）才拒绝 | 通用 |
| 1 | 来者不拒，永远说"有" | 稀疏大数组的科学计算等 |
| 2 | 严格记账：总承诺不得超过 CommitLimit | 要求"malloc 成功就绝不 OOM"的场景 |

模式 2 的额度公式（用比例配置时）：

\[ CommitLimit = SwapTotal + MemTotal \times \frac{overcommit\_ratio}{100} \]

默认 ratio 是 50，即"swap 全量 + 一半物理内存"。`/proc/meminfo` 里 `Committed_AS` 是当前总承诺量，模式 2 下超过 CommitLimit 的分配会直接失败（malloc 返回 NULL），换来的是已成功的分配不会被 OOM 赖账。`/proc/sys/vm/overcommit_kbytes` 是按绝对值配置的另一种写法，与 ratio 互斥。

注意模式 2 也有代价：fork 一个大进程可能因为承诺量翻倍而失败，redis 这类依赖 fork 做持久化的服务因此常配模式 1。

## 9. 读懂 /proc/meminfo

排障第一步是 `free -h`，它的数据全部来自 `/proc/meminfo`。关键字段：

| 字段 | 含义 | 排障要点 |
|------|------|----------|
| MemTotal | 可用物理内存总量 | 比标称小是正常的（内核自身占用） |
| MemFree | 完全没人用的页 | **小不代表缺内存** |
| MemAvailable | 估算"不触发 swap 还能给新应用多少" | **判断内存余量看它**（3.14+） |
| Buffers / Cached | 块设备缓存 / 页缓存 | 大部分可回收，属于"被利用"而非"被浪费" |
| Shmem | tmpfs + 共享内存 | 计入 Cached 但无 swap 不可回收 |
| AnonPages | 进程匿名页 | 无 swap 时这块基本是刚性占用 |
| Dirty / Writeback | 待写回 / 正在写回 | 持续偏高说明写盘跟不上 |
| Slab / SReclaimable / SUnreclaim | 内核对象缓存 | SUnreclaim 持续增长才值得怀疑内核泄漏 |
| SwapTotal / SwapFree | swap 总量 / 剩余 | SwapFree 快速下降伴随 si/so 高 = 危险 |
| CommitLimit / Committed_AS | 承诺额度 / 已承诺量 | 仅模式 2 强制；模式 0/1 下也可作参考 |

**MemFree 与 MemAvailable 的区别是必考题**：MemFree 只数完全空闲的页；MemAvailable 在此基础上加上"可快速回收的页缓存和 SReclaimable"，再扣掉水位线保留量，是给运维看的"真实余量"。一台健康的文件服务器 MemFree 很小、MemAvailable 很大，完全正常。

## 10. OOM killer：最后的选择与牺牲者

### 10.1 触发条件

分配路径已经做完直接回收、规整也救不了、还是凑不出页时，内核调用 OOM killer。注意两个边界：

- **malloc 返回 NULL 不等于发生 OOM kill**：那可能只是 overcommit 检查拒绝或地址空间耗尽；OOM kill 是物理内存兑现失败时的动作。
- **cgroup OOM 与全局 OOM 是两回事**：容器（memcg）达到自己的 memory.max 时只在该 cgroup 内部选牺牲者，宿主机可能还很空闲。容器里 `free` 看到的是宿主机数据，排障必须看 cgroup 的限额与用量。

### 10.2 怎么选牺牲者

内核给每个候选任务算一个 **badness 分数（0~1000）**：大致是"该进程的 RSS + swap 占用，占它被允许使用的内存的比例"。用满全部允许内存记 1000，用一半记 500。root 进程额外多算 3% 的额度（略微降低其分数）。"允许的内存"取决于 OOM 场景：全局 OOM 按全系统算，memcg OOM 按该组限额算。

管理员可用 `/proc/<pid>/oom_score_adj` 干预，取值 **-1000 ~ +1000**，直接加到 badness 上：

- `+1000`：几乎必死，适合"出了事优先杀我"的批处理任务；
- `-1000`：badness 恒为 0，**完全免死**，适合 sshd 这类保命通道；
- 调高自己的值不需要特权；调低需要 CAP_SYS_RESOURCE；
- 子进程继承父进程的 adj 值；命令行工具 `choom` 可以直接查看/设置；
- 老接口 `oom_adj`（-17 免死）已废弃，面试提一句即可，别再用。

选中后发的是 **SIGKILL**：不可捕获、不可忽略，进程没有任何机会执行清理逻辑（缓冲没刷盘就是没了）。这也是"重要服务要自己做好内存上限，而不是指望被杀前留遗言"的原因。

### 10.3 相关旋钮与现场取证

| 参数 | 作用 | 默认 |
|------|------|------|
| vm.panic_on_oom | 0=杀进程保系统；1/2=直接 panic（配 kdump 抓现场，集群 failover 用） | 0 |
| vm.oom_kill_allocating_task | 非 0 时不扫描全任务表，直接杀触发 OOM 的那个 | 0 |
| vm.oom_dump_tasks | OOM 时把所有任务的内存快照打进内核日志 | 1 |

事后排障三板斧（全部只读）：

```bash
# 1. 找到 OOM 记录：谁触发、谁被杀、当时各进程占多少
dmesg | grep -i -E "out of memory|oom|killed process"
# 2. 看某进程当前的 badness 与人为调整值
cat /proc/<pid>/oom_score /proc/<pid>/oom_score_adj
# 3. 容器场景：确认是不是 memcg OOM（日志里会有 memory cgroup out of memory 字样）
```

日志里 `Killed process 1234 (java) total-vm:... anon-rss:...` 这一行就是"死亡证明"，anon-rss 大说明是匿名内存吃满，配合第 9 节的字段能还原案发前的内存构成。

## 11. 只读观察命令速查

全部只读，随便贴：

```bash
free -h                          # 总量视图，看 available 列
cat /proc/meminfo                # 详细字段
cat /proc/buddyinfo              # 各 order 空闲块，判断外部碎片
sudo cat /proc/slabinfo | head   # 内核对象缓存明细
sudo slabtop -o | head -20       # slab 占用排行
swapon --show                    # swap 设备与用量
vmstat 1                         # si/so 换入换出速率
cat /proc/pressure/memory        # PSI 内存压力
cat /proc/vmstat | grep -E "allocstall|pgscan|pgsteal"   # 回收活动计数
cat /proc/self/oom_score         # 当前 shell 的 badness
```

会改系统状态的命令（**慎用**，实验机再动）：`mkswap/swapon/swapoff`、`sysctl vm.swappiness=...`、`sysctl vm.overcommit_memory=...`、`echo 3 > /proc/sys/vm/drop_caches`、`echo 1 > /proc/sys/vm/compact_memory`、`choom -n -500 -p <pid>`。

## 12. 实验一：观察 malloc 与真实物理内存

这个实验把第 5、8 节的结论亲手验证一遍：malloc 只涨虚拟内存（VmSize），逐页写入才涨物理内存（VmRSS）。

```c
// overcommit_rss.c —— 观察 "malloc 成功 != 占用物理内存"
// 编译：gcc -Wall -o overcommit_rss overcommit_rss.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 从 /proc/self/status 里挑出 VmSize / VmRSS 两行打印（单位 kB）
static void print_mem(const char *stage)
{
    FILE *fp = fopen("/proc/self/status", "r");
    if (!fp) { perror("fopen"); return; }
    char line[256];
    printf("== %s ==\n", stage);
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmSize:", 7) == 0 ||
            strncmp(line, "VmRSS:", 6) == 0) {
            printf("%s", line);   // 原样输出该行
        }
    }
    fclose(fp);
}

int main(void)
{
    // 256MB；小内存机器/容器请改小到 64MB，避免给系统制造真实压力
    const size_t SZ = 256UL * 1024 * 1024;

    print_mem("malloc 之前");

    char *p = malloc(SZ);          // 只划走虚拟地址空间，物理页还没给
    if (!p) { perror("malloc"); return 1; }
    print_mem("malloc 之后（还没摸过）");   // 预期：VmSize 涨 256MB，VmRSS 几乎不动

    for (size_t i = 0; i < SZ; i += 4096) {
        p[i] = 1;                  // 每页写 1 字节：触发缺页，物理页此刻才真正分配
    }
    print_mem("逐页写入之后");     // 预期：VmRSS 也涨了约 256MB

    free(p);                       // 大块内存 glibc 通常直接 munmap 还给内核
    print_mem("free 之后");        // 预期：两者都回落
    return 0;
}
```

运行与观察点：

1. 四个阶段的 VmSize/VmRSS 对比，直接印证"读零页、写才分配"。
2. 开第二个终端跑 `vmstat 1`，写入阶段能看到 MemFree 下降；如果机器内存很小且无 swap，这一步就可能触发回收甚至 OOM——**这正是 overcommit 的兑现时刻**。
3. 把写入循环改成只读（`volatile char c = p[i];`），再跑一遍：RSS 几乎不涨，因为读缺页只映射共享零页。

## 13. 实验二：查看并调整自己的 OOM 分数

```c
// oom_score.c —— 查看并调高自己的 OOM badness（只影响本进程，安全）
// 编译：gcc -Wall -o oom_score oom_score.c
#include <stdio.h>

// 读一个只含整数的 proc 文件
static long read_num(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) { perror(path); return -1; }
    long v = -1;
    if (fscanf(fp, "%ld", &v) != 1) v = -1;   // 读取失败按 -1 处理
    fclose(fp);
    return v;
}

int main(void)
{
    printf("调整前 oom_score     = %ld\n", read_num("/proc/self/oom_score"));
    printf("调整前 oom_score_adj = %ld\n", read_num("/proc/self/oom_score_adj"));

    // 规则：调高自己的 adj 不需要任何特权；调低才需要 CAP_SYS_RESOURCE
    FILE *fp = fopen("/proc/self/oom_score_adj", "w");
    if (!fp) { perror("open oom_score_adj"); return 1; }
    if (fprintf(fp, "%d", 200) < 0) {   // +200：让自己在 OOM 时更容易被选中
        perror("write");
        fclose(fp);
        return 1;
    }
    fclose(fp);

    printf("调整后 oom_score_adj = %ld\n", read_num("/proc/self/oom_score_adj"));
    printf("调整后 oom_score     = %ld\n", read_num("/proc/self/oom_score"));
    return 0;
}
```

运行与观察点：

1. adj 从 0 变 200 后，oom_score 明显上升；这个分数只在 OOM 选人时起作用，平时对进程没有任何影响。
2. 试着把 200 改成 -200 再用普通用户跑：写入会失败（权限不足），印证"降低免死程度需要特权"。
3. 用 `choom -n 300 -- ./你的程序` 可以在不改代码的情况下给任意命令设置 adj，效果等价。

## 14. 踩坑清单

1. **把 malloc 成功当内存充足**：默认 overcommit 下 malloc 几乎不失败，真正的失败发生在写入触页时，形式是被 OOM 杀，而不是返回 NULL。
2. **看到 MemFree 小就喊内存不够**：页缓存本来就该把空闲内存用满，判断余量看 MemAvailable / free 的 available 列。
3. **以为 swappiness=0 禁用 swap**：0 只是强烈倾向不换，水位吃紧照样换；禁用要 swapoff。
4. **把 swap 当内存扩容**：热数据被换出去就是颠簸，延迟雪崩比 OOM 更难排查。
5. **给所有进程都设 oom_score_adj=-1000**：全都免死等于 OOM 时无人可杀，系统可能直接僵死或 panic。免死名额只给 sshd 这类保命通道。
6. **还在用 oom_adj**：老接口已废弃移除，一律用 oom_score_adj（-1000~1000）。
7. **生产环境定时 drop_caches "释放内存"**：丢的是热缓存，接下来一波磁盘 IO 和 CPU 重建缓存，性能反而抖动；它只适合测试对比。
8. **Slab 占用高就断定内核泄漏**：先看 SReclaimable（dentry/inode 可回收，属正常缓存），SUnreclaim 持续单调增长才值得追。
9. **buddyinfo 高 order 全 0 就断定没内存**：那是碎片不是缺页，低 order 还很富裕；先想规整，别急着重启。
10. **忘了 tmpfs/shmem 的特殊性**：它算在 Cached 里，但没有 swap 时一分钱都回收不了；往 /dev/shm 写大文件等于直接吃内存。
11. **容器里看宿主机 free 排障**：memcg OOM 只看组内限额，宿主机的 available 再大也救不了超限的容器。
12. **模式 2 下大进程 fork 失败**：严格记账把承诺量算双份，redis 等依赖 fork 的服务要评估后再选模式。
13. **忽视 mlock/unevictable 页**：锁住的页不参与回收和换出，密码库、DPDK 之类用得多时刚性占用会超预期。
14. **指望被 OOM 杀前能保存现场**：SIGKILL 不可捕获，析构、atexit、信号处理器统统不执行；关键数据要平时就落盘。
15. **乱调 min_free_kbytes**：太小系统在高负载下易死锁，太大等于人为压缩可用内存直接触发 OOM，没有测量就别动。

## 15. 面试真题

**1. 什么是伙伴系统？它怎么工作？**

伙伴系统是 Linux 的物理页分配器，把空闲内存按 2 的幂（order 0~10，1 页到 1024 页）组织成链表。分配时找不到刚好大小的块就从更大的块对半劈，释放时检查同大小、物理相邻且对齐的"伙伴"是否空闲，是就逐级合并。它用简单的分裂合并规则换来了快速的连续物理内存分配，代价是外部碎片和向上取整的内部碎片。

**2. 内核为什么在伙伴系统之上还要 slab？**

内核对象大多远小于一页，直接按页分配内部碎片严重，而且高频分配释放反复劈页合页开销大。slab 为每类对象建缓存，从伙伴系统批发页面切成等大小格子，释放的格子留着复用，省去重复初始化。一句话：伙伴管"页"，slab 管"页内的小对象"。

**3. SLAB 和 SLUB 有什么区别？**

SLUB 是 SLAB 的简化重写，去掉了复杂的多级队列，把管理元数据内嵌到页结构里，内存开销更小、代码更简单，还带完善的调试特性。两者对上层接口一致，SLUB 是当前内核默认。老的 SLOB 面向极小内存系统，已在较新内核中移除。

**4. 内部碎片和外部碎片分别怎么解决？**

内部碎片是"分到的比要的大"，伙伴系统按 2 的幂向上取整会产生，小对象交给 slab 分档缓解。外部碎片是"空闲总量够但不连续"，Linux 靠按迁移类型分组存放和内存规整（搬移可移动页聚出连续区间）应对。虚拟内存本身也让普通应用不依赖物理连续，从根上降低了外部碎片的杀伤力。

**5. 页缓存和匿名页回收方式有什么不同？**

页缓存有磁盘上的原文件做备份：干净页直接丢，脏页写回文件后丢。匿名页没有备份文件，必须先换出到 swap 才能释放物理页。所以无 swap 系统上匿名页基本不可回收，内存压力全压在页缓存上，这也是无 swap 机器更容易 OOM 的原因。

**6. kswapd 什么时候被唤醒？直接回收是什么？**

zone 空闲页跌破 low 水位时唤醒 kswapd，异步回收直到回到 high 水位以上，应用无感知。如果消耗太快跌破 min 水位，分配者自己进入直接回收，同步扫描腾页，分配调用被阻塞，表现为延迟毛刺。排障看 /proc/vmstat 的 allocstall 计数和 PSI 内存压力。

**7. swappiness 是什么？设 0 就不 swap 了吗？**

它表示换出匿名页相对回收文件页的 IO 成本比，范围 0~200、默认 60，越低越倾向回收文件页而不是换匿名页。设 0 只是尽量不换，空闲加文件页跌破 high 水位时仍会 swap，并非禁用。zram/zswap 这类内存压缩 swap 反而应该设到 100 以上。

**8. 为什么 malloc 成功了进程还会被 OOM 杀掉？**

默认 overcommit 策略下 malloc 只登记虚拟地址空间，不预留物理页，所以几乎总能成功。真正写入时缺页中断才去拿物理页，此时若回收和 swap 都腾不出来，内核触发 OOM killer 发 SIGKILL。一句话：malloc 是打白条，触页是兑现，OOM 是兑现失败的强制清算。

**9. overcommit 三种模式各是什么？**

0 是默认启发式，只拒绝明显超过 RAM+swap 的离谱申请；1 是永远允许，适合稀疏使用大内存的程序；2 是严格记账，总承诺不超过 CommitLimit（swap + 物理内存乘以比例，默认 50%），超了 malloc 直接失败。模式 2 用"可能分配失败"换"分配成功就绝不赖账"，但会影响依赖 fork 的服务。

**10. OOM killer 怎么选进程？怎么保护关键服务？**

按 badness 打分：进程 RSS 加 swap 占用占其允许内存的比例，0~1000，root 略优待。管理员用 oom_score_adj（-1000~1000）加减分，-1000 完全免死，+1000 优先牺牲，调低需要特权。保护 sshd 设 -1000、给可牺牲的批处理设正值是标准做法，但别把所有人都设成免死。

**11. MemFree 和 MemAvailable 有什么区别？**

MemFree 只统计完全空闲的页。MemAvailable 是内核估算的"不触发 swap 还能给新应用多少"，把可快速回收的页缓存、SReclaimable 算进来再扣掉水位保留。健康的缓存型服务器 MemFree 很小而 MemAvailable 很大，判断余量必须看后者。

**12. 容器 OOM 和宿主机 OOM 有什么区别？**

容器进程受 memcg 限额约束，组内用量达到上限就触发组内 OOM，只在容器里选牺牲者，宿主机可能还很空闲。全局 OOM 是整台机器物理内存耗尽，候选是所有进程。所以容器被杀先看 cgroup 限额和日志里的 memory cgroup 字样，别拿宿主机 free 下结论。

**13. 线上能用 drop_caches 释放内存吗？**

技术上能：它丢弃干净页缓存和可回收 slab，且不破坏数据。但丢的是热缓存，之后大量请求穿透到磁盘，IO 和 CPU 飙升，性能反而恶化。它是测试工具不是优化手段，线上"内存被缓存占用"本来就是正常状态，不需要也不应该定期清。

## 16. 参考资料

本章知识点覆盖对照了以下公开资料，正文与示例代码为作者原创重写，未整段复制原文：

- Linux 内核文档 Concepts overview：页缓存、匿名内存、回收、规整与 OOM 总览 —— https://docs.kernel.org/admin-guide/mm/concepts.html
- Linux 内核文档 /proc/sys/vm/：overcommit、swappiness、水位线、panic_on_oom 等参数 —— https://docs.kernel.org/admin-guide/sysctl/vm.html
- Linux 手册 proc_meminfo(5)：/proc/meminfo 各字段含义 —— https://man7.org/linux/man-pages/man5/proc_meminfo.5.html
- Linux 手册 proc_pid_oom_score_adj(5)：badness 打分与 adj 调整规则 —— https://man7.org/linux/man-pages/man5/proc_pid_oom_score_adj.5.html
- Linux 手册 slabinfo(5)：slab 缓存统计与列含义 —— https://man7.org/linux/man-pages/man5/slabinfo.5.html

## 17. 上一节 | 下一节

- 上一节：[5.2 进程调度](../02-scheduling/README.md)
- 下一节：5.4 死锁与并发控制（待写）

> 作者：青松与桑叶。内存题的钥匙是一条供给链：伙伴批页、slab 切格、缓存与匿名页入住、水位触发腾退、swap 兜底、OOM 清算。把"谁在申请、谁在回收、回收不动时杀谁"这条线讲顺，比背任何一个参数默认值都值钱。
