# 4.5 Shell 与自动化

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

## 目录

- [1. Shell 自动化到底解决什么问题](#1-shell-自动化到底解决什么问题)
- [2. 写脚本前必须掌握的基础](#2-写脚本前必须掌握的基础)
- [3. 一个可直接运行的健壮脚本](#3-一个可直接运行的健壮脚本)
- [4. 文本处理与批量操作](#4-文本处理与批量操作)
- [5. cron：最简单的定时任务](#5-cron最简单的定时任务)
- [6. systemd service：把程序交给系统管理](#6-systemd-service把程序交给系统管理)
- [7. systemd timer：更可靠的定时调度](#7-systemd-timer更可靠的定时调度)
- [8. OpenRC：另一类初始化系统](#8-openrc另一类初始化系统)
- [9. logrotate：日志切割与归档](#9-logrotate日志切割与归档)
- [10. 底层原理与踩坑清单](#10-底层原理与踩坑清单)
- [11. 面试真题](#11-面试真题)
- [12. 参考资料](#12-参考资料)
- [13. 上一节 | 下一节](#13-上一节--下一节)

---

## 1. Shell 自动化到底解决什么问题

你每天都在终端里执行命令：查日志、清临时文件、备份目录、启动服务。只要一组操作需要重复执行，就应该考虑把它写成脚本，而不是靠记忆和手速。

Shell 自动化通常分四层：

| 层次 | 解决的问题 | 常用工具 |
|---|---|---|
| 命令封装 | 把重复操作写成可复用程序 | Bash、find、grep、awk、sed |
| 定时触发 | 到点自动执行 | cron、systemd timer |
| 进程托管 | 开机启动、异常重启、日志与权限管理 | systemd、OpenRC |
| 日志治理 | 防止日志无限增长，保留必要历史 | logrotate、journald |

面试时不要把 Shell 说成“会几个 Linux 命令”。更好的表述是：**Shell 负责把系统命令组织成可靠流程，cron/timer 负责触发，init 系统负责托管，logrotate 负责控制日志生命周期。**

下面所有脚本默认使用 Bash。涉及修改系统配置的命令只用于演示配置方法，不要在生产服务器上不看内容就直接执行。

---

## 2. 写脚本前必须掌握的基础

### 2.1 shebang、权限与执行方式

脚本第一行通常写：

```bash
#!/usr/bin/env bash
```

它告诉操作系统：用 `PATH` 中找到的 Bash 解释这个文件。随后可以执行：

```bash
chmod +x demo.sh       # 给脚本增加执行权限
./demo.sh              # 通过 shebang 启动一个新的 Bash 进程
bash demo.sh           # 显式交给 Bash，不要求文件有执行权限
source demo.sh         # 在当前 Shell 中执行，会改变当前目录、变量和选项
```

`./demo.sh`、`bash demo.sh` 与 `source demo.sh` 的最大区别是：前两者通常在新进程中运行，脚本里的 `cd`、`export`、`set` 不影响父 Shell；`source` 在当前 Shell 中运行，会影响当前环境。

### 2.2 变量与引用：大部分空格问题都出在这里

```bash
name="hello world"

printf '%s\n' "$name"   # 正确：一个参数，保留空格
printf '%s\n' $name     # 不推荐：会经过分词和通配符展开
```

记住一个默认习惯：**使用变量时先加双引号，除非你明确需要分词或通配符展开。**

常见特殊变量：

| 写法 | 含义 |
|---|---|
| `$0` | 脚本名或当前 Shell 名 |
| `$1`、`$2` | 第 1、2 个位置参数 |
| `$#` | 参数个数 |
| `"$@"` | 每个参数仍是独立参数，转发参数时首选 |
| `"$*"` | 把所有参数拼成一个字符串 |
| `$?` | 最近一条前台命令的退出状态 |
| `$$` | 当前 Shell 的 PID |

退出状态是 Shell 自动化最重要的“返回值”：**0 表示成功，非 0 表示失败。** 常见值还有 126（找到命令但不能执行）、127（命令不存在）、128+N（被编号为 N 的信号终止）。

### 2.3 `set -Eeuo pipefail` 有什么用

生产脚本常见开头：

```bash
#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'
```

| 选项 | 作用 | 注意事项 |
|---|---|---|
| `-e` | 某些命令失败时立即退出脚本 | 不是“任何失败都退出”，`if`、`&&`、`||`、`!` 等上下文有例外 |
| `-E` | 让 `ERR` trap 在函数、子 Shell、命令替换中继承 | 只控制 ERR trap，不等于加强 `-e` 的所有语义 |
| `-u` | 使用未定义变量时报错 | 可选参数要写 `${name:-默认值}` |
| `pipefail` | 管道中任一命令失败，整个管道可返回失败 | 默认只看管道最后一个命令的状态 |
| `IFS=$'\n\t'` | 去掉默认 IFS 中的空格，减少意外分词 | 不能替代正确引用 |

例如，默认情况下下面的管道可能返回成功，因为 `cat` 失败了，但 `wc -l` 成功了：

```bash
cat not-exist.txt | wc -l
printf '默认管道状态：%s\n' "$?"
```

开启 `pipefail` 后，管道可以反映中间环节的失败。若确实要逐步检查，还可以读取 Bash 的 `PIPESTATUS` 数组。

### 2.4 函数、返回值与 trap

Shell 函数的 `return 0` 到 `return 255` 返回的是**退出状态**，不是普通数据。要传出字符串，通常用 `printf` 输出，再由调用方用命令替换接收。

```bash
make_report() {
    local dir="$1"             # local 避免污染全局变量
    printf '正在检查目录：%s\n' "$dir" >&2  # 日志写 stderr，避免混入结果
    find "$dir" -maxdepth 1 -type f | wc -l   # 结果写 stdout
}

count=$(make_report "/tmp")   # 接收 stdout 中的结果
printf '文件数：%s\n' "$count"
```

`trap` 用来在收到信号或脚本退出时执行清理：

```bash
tmp_file=$(mktemp)                 # 创建权限安全的临时文件
trap 'rm -f -- "$tmp_file"' EXIT   # 无论正常结束还是出错退出，都尝试清理
```

这里的单引号很重要：让 `$tmp_file` 在 trap 真正执行时展开，而不是注册 trap 时就展开。

---

## 3. 一个可直接运行的健壮脚本

下面的脚本统计指定目录第一层的普通文件，生成清单，并用 `flock` 防止两个实例同时运行。保存为 `make-report.sh`：

```bash
#!/usr/bin/env bash
set -Eeuo pipefail
IFS=$'\n\t'

# 第一个参数是要扫描的目录，默认当前目录。
# 第二个参数是报告路径，默认写到当前目录 report.txt。
readonly SRC_DIR="${1:-$PWD}"
readonly REPORT="${2:-$PWD/report.txt}"

if [[ ! -d "$SRC_DIR" ]]; then
    printf '错误：目录不存在：%s\n' "$SRC_DIR" >&2
    exit 2
fi

# 文件描述符 9 指向锁文件；flock 锁住的是打开文件描述，不是路径字符串。
exec 9>"${REPORT}.lock"
if ! flock -n 9; then
    printf '已有实例正在运行，本次直接退出。\n' >&2
    exit 0
fi

# 临时文件与报告放在同一目录，最后 rename 时不会跨文件系统。
tmp_file=$(mktemp "${REPORT}.tmp.XXXXXX")
trap 'rm -f -- "$tmp_file"' EXIT

{
    printf '扫描目录：%s\n' "$SRC_DIR"
    printf '生成时间：%s\n' "$(date '+%F %T')"
    printf '%s\n' '-------- 文件清单 --------'
    # -printf 是 GNU find 的扩展；macOS/BSD find 需要换写法。
    find "$SRC_DIR" -maxdepth 1 -type f -printf '%f\n' | LC_ALL=C sort
} >"$tmp_file"

# 读者要么看到旧报告，要么看到完整新报告，避免看到写了一半的文件。
mv -f -- "$tmp_file" "$REPORT"
trap - EXIT

printf '报告已生成：%s\n' "$REPORT"
```

运行方式：

```bash
chmod +x make-report.sh
./make-report.sh /tmp /tmp/demo-report.txt
cat /tmp/demo-report.txt
```

这段脚本包含四个工程要点：

1. **参数检查**：目录不存在时给出明确错误并返回非 0。
2. **防重入**：锁拿不到就退出，避免定时任务越堆越多。
3. **清理临时文件**：`EXIT` trap 覆盖正常退出和多数异常退出。
4. **原子替换**：先写临时文件，再 `mv` 替换正式报告。

注意：`mv` 是否原子取决于具体场景。同一路径所在文件系统内的 rename 通常可以提供原子替换；跨文件系统移动则可能退化为复制加删除，因此示例把临时文件建在报告旁边。

---

## 4. 文本处理与批量操作

### 4.1 文件名有空格，优先使用 NUL 分隔

下面写法容易在空格、换行文件名上出错：

```bash
# 不推荐：按空白拆分 find 的输出
for file in $(find /tmp -type f); do
    printf '%s\n' "$file"
done
```

更稳妥的写法：

```bash
# find 用 NUL 分隔路径，read -d '' 按 NUL 读取。
# 进程替换让 while 在当前 Shell 中运行，循环内修改的变量不会丢。
count=0
while IFS= read -r -d '' file; do
    count=$((count + 1))
    printf '第 %d 个文件：%s\n' "$count" "$file"
done < <(find /tmp -maxdepth 1 -type f -print0)
printf '合计：%d\n' "$count"
```

如果写成 `find ... | while read ...; do ...; done`，默认情况下 while 可能运行在子 Shell 中，循环里增加的 `count` 在管道结束后就消失了。这是 Shell 面试和实际排障都很常见的坑。

### 4.2 grep、awk、sed 怎么分工

| 工具 | 最擅长的事 | 示例 |
|---|---|---|
| grep | 按模式筛选行 | `grep -E 'ERROR|WARN' app.log` |
| awk | 按字段统计与格式化 | `awk '{sum += $1} END {print sum}' nums.txt` |
| sed | 面向行的替换与编辑 | `sed -n '20,40p' app.log` |
| sort/uniq | 排序、去重、计数 | `sort ips.txt | uniq -c | sort -nr` |
| xargs | 把输入组织成命令参数 | 配合 `-0` 处理特殊文件名 |

例如统计 access.log 中各状态码的数量（假设状态码是第 9 列）：

```bash
awk '{count[$9]++} END {for (code in count) print code, count[code]}' access.log \
    | sort -k2,2nr
```

不要为了“显得高级”把简单任务写成超长单行命令。自动化脚本首先要可读、可测、可定位错误。

---

## 5. cron：最简单的定时任务

### 5.1 五段时间字段

用户 crontab 的一行通常是：

```text
分  时  日  月  周  要执行的命令
```

| 字段 | 取值 |
|---|---|
| 分钟 | 0～59 |
| 小时 | 0～23 |
| 每月几号 | 1～31 |
| 月份 | 1～12 |
| 星期几 | 0～7，0 和 7 都表示星期日；也可用名称 |

示例：

```cron
# 每 5 分钟执行一次
*/5 * * * * /usr/local/bin/make-report.sh /var/lib/demo /var/log/demo/report.txt

# 每天 03:30 执行
30 3 * * * /usr/local/bin/make-report.sh /var/lib/demo /var/log/demo/report.txt

# 工作日 09:00 执行
0 9 * * 1-5 /usr/local/bin/make-report.sh /var/lib/demo /var/log/demo/report.txt
```

编辑和查看当前用户任务：

```bash
crontab -e     # 编辑当前用户 crontab
crontab -l     # 列出当前用户任务
```

系统级 `/etc/crontab` 和 `/etc/cron.d/` 通常比用户 crontab 多一个“运行用户”字段，不能机械地把用户 crontab 的五段规则复制过去。

### 5.2 cron 的高频坑

**第一，环境和你登录后的 Shell 不一样。** cron 默认使用 `/bin/sh`，PATH 往往很短，当前目录也不一定是项目目录。解决方法是使用绝对路径，并在 crontab 或脚本内显式设置环境：

```cron
SHELL=/bin/bash
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
MAILTO=""

*/5 * * * * /usr/bin/flock -n /tmp/make-report.lock /usr/local/bin/make-report.sh /var/lib/demo /var/log/demo/report.txt >>/var/log/demo/cron.log 2>&1
```

**第二，命令中的 `%` 有特殊含义。** 未转义的 `%` 会被 cron 当成分隔符，后面的内容可变成命令标准输入。因此写日期格式要转义：

```cron
0 2 * * * /bin/tar -czf /backup/data-$(/bin/date +\%F).tar.gz /var/lib/demo
```

更推荐把复杂命令放进脚本，crontab 只保留脚本绝对路径。

**第三，“日”和“周”同时限制时可能按 OR 匹配。** 传统 cron 规则中，如果“每月几号”和“星期几”都不是 `*`，任意一个匹配就可能执行，而不是必须同时满足。写“每月 1 日且周五”这类规则前，必须核对所用 cron 实现。

**第四，任务执行时间超过调度间隔会重叠。** 用 `flock`、应用内幂等设计或 systemd 的服务运行状态避免重入，不要假设上一轮已经结束。

---

## 6. systemd service：把程序交给系统管理

长期进程如果只用 `nohup ./app &`，重启后不会自动恢复，进程崩溃也没人管。systemd 可以把启动用户、工作目录、环境、重启策略、资源限制和日志统一纳入 unit。

### 6.1 常见 Type 与 Restart

| Type | 典型用途 | 判断“启动完成”的要点 |
|---|---|---|
| `simple` | 前台常驻进程 | fork 出主进程后很快进入后续流程，不代表应用已初始化完成 |
| `exec` | 前台常驻进程 | 等到 exec 成功；能更早发现二进制不存在等问题 |
| `oneshot` | 执行一次就退出的任务 | 等主进程退出；常用于 timer 或初始化动作 |
| `notify` | 支持 sd_notify 的服务 | 等应用主动报告 ready，应用必须实现该协议 |
| `forking` | 传统自行 daemonize 的程序 | 等父进程退出，通常配 PIDFile；新服务一般不必优先选它 |

| Restart | 含义 | 适合场景 |
|---|---|---|
| `no` | 不自动重启 | 一次性任务 |
| `on-failure` | 非零退出、异常信号、超时等失败时重启 | 大多数常驻服务 |
| `always` | 不论正常或异常退出都重启 | 明确需要一直运行的服务 |
| `on-abnormal` | 只在异常信号、看门狗超时等情况重启 | 不希望正常退出后重启的服务 |

不要把 `Type=oneshot` 的定时任务配上 `Restart=always`：一次性任务与持续重启的模型不匹配。

### 6.2 一个常驻服务 unit 示例

假设应用是 `/opt/demo/bin/server`，它保持前台运行，配置如下：

```ini
# /etc/systemd/system/demo-agent.service
[Unit]
Description=Demo Agent
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=demo
Group=demo
WorkingDirectory=/opt/demo
ExecStart=/opt/demo/bin/server --config /opt/demo/config.ini
Restart=on-failure
RestartSec=5s
TimeoutStopSec=20s

# 基础加固；目录权限必须与实际读写需求匹配。
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/demo /var/log/demo

[Install]
WantedBy=multi-user.target
```

常用管理命令：

```bash
# 修改 unit 后重新加载配置
systemctl daemon-reload

# 设置开机启动，并立即启动；会改变系统服务状态，生产环境慎用
systemctl enable --now demo-agent.service

# 只读排查
systemctl status demo-agent.service
journalctl -u demo-agent.service -n 100 --no-pager
journalctl -u demo-agent.service -f
```

`enable` 管理的是“开机是否启用”，`start` 管理的是“现在是否运行”；`enable --now` 同时做两件事。

---

## 7. systemd timer：更可靠的定时调度

systemd timer 到点后激活一个 service，通常由同名的 `.timer` 激活 `.service`。它既能按日历时间触发，也能按开机后的相对时间触发。

### 7.1 一次性任务 service

```ini
# /etc/systemd/system/demo-report.service
[Unit]
Description=Generate Demo Report

[Service]
Type=oneshot
User=demo
Group=demo
ExecStart=/usr/local/bin/make-report.sh /var/lib/demo /var/log/demo/report.txt

# 一次性任务完成后就退出，不需要 Restart。
NoNewPrivileges=true
PrivateTmp=true
```

### 7.2 配套 timer

```ini
# /etc/systemd/system/demo-report.timer
[Unit]
Description=Run Demo Report Every Day

[Timer]
OnCalendar=*-*-* 03:30:00
Persistent=true
AccuracySec=1min
RandomizedDelaySec=5min

[Install]
WantedBy=timers.target
```

几个关键选项：

| 选项 | 作用 |
|---|---|
| `OnCalendar=` | 按日历时间触发，例如每天 03:30 |
| `OnBootSec=` | 开机后经过指定时间触发 |
| `OnUnitActiveSec=` | 相对关联 unit 上次激活的时间触发 |
| `Persistent=true` | 对日历 timer 补跑停机期间错过的任务 |
| `AccuracySec=` | 允许系统合并唤醒的时间窗口，影响触发精度 |
| `RandomizedDelaySec=` | 随机延迟，避免大量任务同一时刻挤爆资源 |

启用与排查：

```bash
systemctl daemon-reload
systemctl enable --now demo-report.timer   # 启用 timer，不是常驻 report service

systemctl list-timers demo-report.timer   # 查看下次/上次触发时间
systemctl status demo-report.service     # 查看本次任务结果
journalctl -u demo-report.service -n 100 --no-pager
```

### 7.3 cron 与 systemd timer 对比

| 对比项 | cron | systemd timer |
|---|---|---|
| 配置成本 | 一行即可，简单 | 通常需要 service + timer 两个 unit |
| 日志 | 邮件或自行重定向 | 可直接进入 journal，按 unit 查询 |
| 错过任务补跑 | 传统 cron 通常不负责 | `Persistent=true` 可补跑日历任务 |
| 依赖、权限、资源控制 | 较弱，需脚本自己处理 | 可使用 unit 的依赖与沙箱能力 |
| 适用 | 简单、传统、跨 Unix 环境 | 已使用 systemd 的 Linux 服务器 |

---

## 8. OpenRC：另一类初始化系统

Alpine Linux、Gentoo 等环境可能使用 OpenRC，而不是 systemd。它同样负责服务依赖、启动停止和运行级别，但配置形式不同。

下面是一个简化示例：

```bash
#!/sbin/openrc-run
# /etc/init.d/demo-agent

description="Demo Agent"
command="/opt/demo/bin/server"
command_args="--config /opt/demo/config.ini"
command_user="demo:demo"
command_background="yes"
pidfile="/run/${RC_SVCNAME}.pid"

depend() {
    need net          # 启动本服务前需要 net 服务
    after firewall    # 有 firewall 时排在其后
}
```

常用命令：

```bash
chmod +x /etc/init.d/demo-agent
rc-update add demo-agent default   # 加入 default 运行级别，开机启动
rc-service demo-agent start       # 立即启动
rc-service demo-agent status      # 查看状态
rc-service demo-agent stop        # 停止
```

| 对比项 | systemd | OpenRC |
|---|---|---|
| 服务定义 | `.service` 等 unit 文件 | `/etc/init.d/` 下的 openrc-run 脚本 |
| 立即启动/停止 | `systemctl start/stop` | `rc-service start/stop` |
| 开机启动 | `systemctl enable` | `rc-update add ... default` |
| 日历定时 | 原生 systemd timer | 通常配合 cron 等外部调度器 |
| 常见环境 | 多数主流服务器发行版 | Alpine、Gentoo 等 |

面试不必背两套所有参数，但要知道：**unit 文件不是 Unix 通用标准，换发行版或容器基础镜像时，先确认 PID 1 和 init 系统。**

---

## 9. logrotate：日志切割与归档

日志文件一直追加，最终会吃满磁盘。logrotate 可以按时间或大小轮转日志，并负责保留份数、压缩、缺失处理和轮转后脚本。

### 9.1 配置示例

```conf
# /etc/logrotate.d/demo
/var/log/demo/*.log {
    daily                 # 每天检查并轮转
    rotate 14             # 最多保留 14 份旧日志
    compress              # 压缩旧日志
    delaycompress         # 再延后一份压缩，减少刚轮转文件仍被读取的影响
    missingok             # 文件不存在不报错
    notifempty            # 空文件不轮转
    create 0640 demo adm  # 轮转后创建新日志文件及权限
    sharedscripts         # 通配符匹配多个日志时，postrotate 只执行一次
    postrotate
        # 前提：应用支持收到 HUP 后关闭并重新打开日志。
        /usr/bin/systemctl kill -s HUP demo-agent.service >/dev/null 2>&1 || true
    endscript
}
```

先做语法和流程预演：

```bash
logrotate -d /etc/logrotate.d/demo   # debug 模式，不真正轮转
```

`-d` 只是预演；不要使用 `-f` 强推生产配置来“试试看”，那会实际改变日志文件。

### 9.2 `create + 重新打开` 与 `copytruncate`

| 方案 | 基本过程 | 优点 | 代价与风险 |
|---|---|---|---|
| rename + create + 通知应用重开 | 旧文件改名，创建新文件，让应用换 fd | 正常流程下不易丢失日志 | 应用必须支持 reopen；不支持时会继续写旧 inode |
| `copytruncate` | 复制旧文件，再把原文件截断为 0 | 应用不用重新打开日志 | 复制与截断之间存在窗口，可能丢少量日志；大文件还会额外占空间和 IO |

如果服务不支持重开日志，也不要直接 `rm app.log`：进程仍持有原文件的 fd，可能继续写入已删除的 inode，磁盘空间不会按你的预期释放，日志也难以查看。

---

## 10. 底层原理与踩坑清单

### 10.1 为什么重命名日志后进程还写旧文件

Linux 进程写日志时，真正持有的是指向打开文件描述的文件描述符。路径改名不会改变这个 fd 与 inode 的关系，所以进程会继续写改名后的文件；删除路径也不会立刻释放仍被引用的 inode。

因此日志切割的核心不是“文件名叫什么”，而是：**应用当前 fd 指向哪个 inode，何时关闭并重新打开。**

### 10.2 cron、service、timer 的关系

- cron 到点直接启动命令，应用自己要处理环境、锁、日志和退出码。
- timer 到点激活 service，service 描述实际运行方式。
- `Type=oneshot` 的 service 退出后进入 inactive/dead 不代表任务失败，关键看退出状态和日志。
- systemd 通过 unit、cgroup 等机制统一托管进程；不是简单替你执行一次 `nohup &`。

### 10.3 踩坑清单

1. **变量不加引号。** 路径含空格或通配符时被错误拆分。
2. **以为 `set -e` 能抓住所有错误。** 条件上下文、管道和命令替换都有细节，关键命令仍要显式判断。
3. **把函数 `return` 当普通返回值。** 它返回退出状态；数据应通过输出、文件或约定变量传递。
4. **管道中的 while 修改变量。** 子 Shell 结束后修改丢失，可改进程替换。
5. **用 `for f in $(find ...)` 批量处理。** 特殊文件名会拆坏，优先 `-print0` + `read -d ''`。
6. **cron 里依赖交互式环境。** PATH、Shell、当前目录不同，使用绝对路径并显式配置环境。
7. **crontab 的 `%` 未转义。** `date +%F` 要写成 `date +\%F`，复杂命令最好放入脚本。
8. **定时任务重叠。** 用 `flock`、幂等设计或服务状态防止重入。
9. **把一次性脚本配置成 `Restart=always`。** 会造成反复执行，应按任务模型选 Type 和重启策略。
10. **只 enable service，却忘了 timer。** 定时任务应启用 `.timer`，不是把 oneshot service 当常驻进程启动。
11. **以为 timer 精到毫秒。** AccuracySec、随机延迟、系统休眠和负载都会影响实际触发时间。
12. **直接删除正在写入的日志。** fd 仍引用旧 inode，既可能丢日志，也可能不释放空间。
13. **盲目使用 copytruncate。** 它方便但有丢失窗口，高价值审计日志应优先设计应用 reopen。
14. **脚本里写账号密码或 token。** 应使用权限受控的独立配置或秘密管理机制，不进入仓库和 crontab 历史。

---

## 11. 面试真题

### Q1：`set -euo pipefail` 分别解决什么问题？

**三句话答案：** `-e` 让部分未处理的命令失败直接终止脚本，`-u` 把使用未定义变量变成错误，`pipefail` 让管道中间命令的失败能够反映到整个管道状态。它不是万能的，`-e` 在 `if`、`&&`、`||` 等上下文有例外。工程上还要配合正确引用、明确错误处理和清理 trap。

### Q2：`"$@"` 和 `"$*"` 有什么区别？

**三句话答案：** `"$@"` 展开后仍保持每个位置参数是独立参数，适合原样转发。`"$*"` 会把所有参数按 IFS 的第一个字符连接成一个字符串。转发带空格的参数时通常必须用 `"$@"`。

### Q3：为什么脚本手工执行正常，放到 cron 里就失败？

**三句话答案：** cron 环境与登录 Shell 不同，默认 Shell、PATH、HOME、当前目录和环境变量都可能不同。相对路径、别名、交互式 Shell 启动文件里的配置不能直接依赖。应使用绝对路径、显式环境、完整日志和明确退出码来定位。

### Q4：cron 的时间字段怎么写，有什么容易忽略的规则？

**三句话答案：** 用户 crontab 通常是“分、时、日、月、周”五段，再加命令。系统级 crontab 常有额外用户字段；命令中的 `%` 要转义。每月几号和星期几同时受限时，传统 cron 可能按 OR 匹配，需要核对具体实现。

### Q5：如何避免定时任务上一轮没结束，下一轮又启动？

**三句话答案：** 可以用 `flock` 对固定锁文件加非阻塞锁，拿不到锁就跳过或等待。分布式任务不能只靠本机 flock，还要使用分布式锁、唯一任务键或数据库幂等约束。还应设置超时、记录退出状态并监控连续失败。

### Q6：systemd 的 simple、oneshot、notify 有什么区别？

**三句话答案：** simple 面向常驻前台进程，启动状态主要围绕主进程创建；oneshot 等进程退出，适合一次性任务。notify 等应用通过 sd_notify 主动报告 ready，能表达“进程存在且初始化完成”。选择错误会导致依赖服务过早启动，或一次性脚本被当成常驻服务管理。

### Q7：`Restart=always` 和 `on-failure` 怎么选？

**三句话答案：** always 不管正常还是异常退出都会拉起，适合明确要求永远运行的服务。on-failure 主要针对非零退出、异常信号和超时等失败，正常退出不重启。一次性任务通常不该配 always，否则可能不断重复执行。

### Q8：systemd timer 相比 cron 有什么优势？

**三句话答案：** timer 与 service 结合，能复用 unit 的用户、依赖、沙箱、资源和日志能力。`Persistent=true` 可补跑停机期间错过的日历任务，journalctl 也便于按 unit 排查。代价是配置更复杂，简单任务用 cron 往往更直接。

### Q9：为什么日志轮转后，磁盘空间没有释放或日志还写进旧文件？

**三句话答案：** 进程的文件描述符仍指向旧 inode，路径改名或删除不会自动让它改指向新文件。需要让应用关闭并重新打开日志，或评估 copytruncate 方案。还要检查是否仍有进程持有已删除文件，可用 `lsof +L1` 等只读手段排查。

### Q10：`copytruncate` 有什么风险？

**三句话答案：** 它先复制日志再截断原文件，应用不必重新打开 fd，所以接入简单。复制与截断之间的新日志可能丢失，大文件复制还会增加磁盘和 IO 压力。对可靠性要求高的日志，优先让应用支持 reopen，并使用 rename/create 的正常轮转流程。

### Q11：OpenRC 和 systemd 的主要差异是什么？

**三句话答案：** systemd 使用 unit 文件管理服务、定时器和依赖，OpenRC 使用 openrc-run 服务脚本和运行级别。前者常见命令是 systemctl，后者是 rc-service 与 rc-update。编写部署脚本前应先确认 init 系统，不能把 systemd unit 当成所有 Linux 都通用。

---

## 12. 参考资料

以下资料用于核对 Bash 参数与退出状态、cron 字段、systemd service/timer 语义以及 logrotate 指令；正文讲解、示例组织与面试答案均为原创重写，没有整段复制原文。

1. bash(1) — Linux manual page：https://man7.org/linux/man-pages/man1/bash.1.html
2. crontab(5) — Linux manual page：https://man7.org/linux/man-pages/man5/crontab.5.html
3. systemd.service — freedesktop：https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html
4. systemd.timer — freedesktop：https://www.freedesktop.org/software/systemd/man/latest/systemd.timer.html
5. logrotate(8) — Linux manual page：https://man7.org/linux/man-pages/man8/logrotate.8.html

---

## 13. 上一节 | 下一节

- 上一节：[4.4 进程间通信](../04-ipc/README.md)
- 下一节：5.1 虚拟内存与页表（待写）

> 作者：青松与桑叶。自动化不是“能跑一次”就结束，而是失败可见、重复执行安全、系统重启后仍能按预期工作。
