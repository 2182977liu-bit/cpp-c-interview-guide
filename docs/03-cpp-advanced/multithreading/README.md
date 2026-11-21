# 3.5 多线程编程

> 作者：青松与桑叶
> 本系列教程定位：保姆级、通俗易懂、每一步都可运行、中文原创

---

## 目录

- 一、并发、并行与多线程
- 二、std::thread：创建线程与管理生命周期
- 三、数据竞争：为什么 counter++ 会算错
- 四、mutex 与 RAII 锁：保护共享数据
- 五、死锁与多把锁的正确打开方式
- 六、条件变量：让消费者睡着等任务
- 七、std::atomic：不可分割的原子操作
- 八、内存序：原子性之外还有执行顺序
- 九、工程实践：线程不是越多越好
- 十、面试真题
- 十一、踩坑提醒

---

## 一、并发、并行与多线程

**并发**是多个任务在一段时间内交替推进，**并行**是多个任务在同一时刻真正同时执行。单核 CPU 可以通过快速切换实现并发，多核 CPU 才能让多个线程真正并行。

进程像一套独立房子，拥有独立地址空间；线程像住在同一套房子里的不同住户，共享代码段、全局变量、堆和打开的文件等资源，但各自拥有栈、寄存器上下文和线程 ID。

多线程适合两类工作：

- **计算密集型**：把大数据切块，让多个 CPU 核心同时计算；
- **IO 密集型**：一个线程等待磁盘或网络时，让其他线程继续处理任务。

但共享内存既方便通信，也带来数据竞争、死锁、可见性和性能抖动。多线程的核心难点不是“如何启动线程”，而是“如何安全地共享状态”。

---

## 二、std::thread：创建线程与管理生命周期

### 2.1 创建线程并传递参数

```cpp
// thread_basic.cpp
// 编译：g++ -std=c++17 -Wall -pthread thread_basic.cpp -o thread_basic
#include <iostream>
#include <string>
#include <thread>
#include <utility>

void worker(int id) {
    std::cout << "worker " << id << " 开始执行\n";
}

void add_suffix(std::string& text, const std::string& suffix) {
    text += suffix;
}

int main() {
    std::thread t1(worker, 1); // 创建后由操作系统调度执行

    std::string text = "task";
    // thread 默认复制/移动保存参数；需要引用语义时使用 std::ref。
    std::thread t2(add_suffix, std::ref(text), std::string{"-done"});

    t1.join(); // 等待 t1 执行结束
    t2.join(); // 等待 t2 修改完成，之后读取 text 才安全
    std::cout << text << '\n'; // task-done
}
```

`std::thread` 表示一个执行线程，禁止复制，只允许移动。构造线程对象后，入口函数就可能立即运行，所以必须提前准备好它要访问的数据。

### 2.2 join、detach 和 joinable

| 操作 | 含义 | 主要风险 |
|---|---|---|
| `join()` | 当前线程等待目标线程结束 | 目标一直不退出，调用方就一直等 |
| `detach()` | 分离句柄，让线程独立运行 | 主程序退出、局部变量销毁后仍访问数据 |
| `joinable()` | 检查对象是否仍关联可汇合线程 | 重复 join/detach 会出错 |

最重要的规则：`std::thread` 对象析构时，如果仍处于 joinable 状态，程序会调用 `std::terminate`。正常业务任务优先明确 `join`；使用 `detach` 前必须证明线程访问的所有数据都比它活得更久。

`join` 不仅用于“等一等”，它还建立了同步关系。子线程完成前的写入，对 join 返回后的线程可见，因此上面的 `text` 可以在 join 后安全读取。

---

## 三、数据竞争：为什么 counter++ 会算错

多个线程同时读写同一个 `int counter`，没有同步就执行 `counter++`，结果经常小于预期。因为自增在逻辑上包含三步：

```text
读取 counter → 寄存器加 1 → 写回 counter
```

两个线程可能同时读到 10，各自算出 11，再先后写回 11，于是两次自增只增加了一次，这叫**丢失更新**。

更严重的是，按 C++ 内存模型，两个线程对同一内存位置产生冲突访问，至少一个是写操作，且没有建立必要的 happens-before 关系，就构成**数据竞争**，行为是未定义的，不只是“结果小一点”。

```cpp
// 错误示意：不要把这个示例当作正确写法
void unsafe_add(int& counter) {
    for (int i = 0; i < 10000; ++i) {
        ++counter; // 多线程同时执行时存在数据竞争
    }
}
```

注意两个常见误区：

- “int 在 x86 上单条指令就能写”不等于 C++ 层面没有数据竞争；
- `volatile` 只处理特定访问语义，不提供原子性、互斥或线程同步，不能拿来修数据竞争。

---

## 四、mutex 与 RAII 锁：保护共享数据

互斥锁保证同一时刻只有一个线程进入临界区。不要手动成对写 `lock()` 和 `unlock()`，而应使用 RAII 包装器，让析构函数负责解锁。

```cpp
// mutex_counter.cpp
// 编译：g++ -std=c++17 -Wall -pthread mutex_counter.cpp -o mutex_counter
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

class Counter {
public:
    void add() {
        std::lock_guard<std::mutex> lock(mutex_); // 离开作用域自动解锁
        ++value_;
    }

    int value() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return value_;
    }

private:
    mutable std::mutex mutex_; // const 成员函数也需要锁定，所以加 mutable
    int value_ = 0;
};

int main() {
    Counter counter;
    std::vector<std::thread> workers;

    for (int id = 0; id < 4; ++id) {
        workers.emplace_back([&counter] {
            for (int i = 0; i < 10000; ++i) {
                counter.add();
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    std::cout << counter.value() << '\n'; // 40000
}
```

### 4.1 常见锁管理器怎么选

| 类型 | 特点 | 适用场景 |
|---|---|---|
| `std::lock_guard` | 构造加锁、析构解锁，不能主动解锁 | 简单临界区，优先使用 |
| `std::unique_lock` | 可延迟加锁、主动解锁、移动所有权 | 条件变量、灵活锁控制 |
| `std::scoped_lock` | C++17 起可同时管理多把互斥锁 | 避免多锁顺序不当造成死锁 |
| `std::shared_lock` | 配合共享互斥量实现读共享 | 读多写少场景 |

锁的保护范围要围绕“数据一致性”设计，而不是只把某一条语句随便包住。读取共享数据也要按同一套锁规则执行，不能只保护写线程。

---

## 五、死锁与多把锁的正确打开方式

死锁的经典情形：线程 A 持有锁 1 等锁 2，线程 B 持有锁 2 等锁 1，双方都无法继续。形成死锁通常需要四个条件：互斥、持有并等待、不可剥夺、循环等待。

```text
线程 A：lock(m1) → 等待 m2
线程 B：lock(m2) → 等待 m1
```

常用预防办法：

1. 所有线程按固定顺序获取锁；
2. 缩短临界区，不在持锁时调用未知的外部回调；
3. 同时需要多把锁时，使用 `std::scoped_lock` 或 `std::lock`；
4. 能不用共享可变状态，就优先使用消息传递或线程私有数据。

```cpp
#include <mutex>

std::mutex first_mutex;
std::mutex second_mutex;
int first = 0;
int second = 0;

void update_both() {
    // C++17：以死锁避免机制同时获得两把锁，离开作用域统一释放。
    std::scoped_lock lock(first_mutex, second_mutex);
    ++first;
    ++second;
}
```

`recursive_mutex` 允许同一线程重复加锁，但它不能修复混乱的所有权设计。正常代码不要依赖递归锁掩盖“函数持锁又调用另一个需要同锁的函数”。

---

## 六、条件变量：让消费者睡着等任务

如果消费者一直写 `while (queue.empty()) {}`，会白白烧 CPU。条件变量让线程在条件不满足时阻塞，生产者修改条件后再通知它。

下面是一个可正常退出的生产者—消费者示例：

```cpp
// condition_queue.cpp
// 编译：g++ -std=c++17 -Wall -pthread condition_queue.cpp -o condition_queue
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>

std::queue<int> tasks;
std::mutex tasks_mutex;
std::condition_variable tasks_cv;
bool stopped = false;

void producer() {
    for (int value = 1; value <= 5; ++value) {
        {
            std::lock_guard<std::mutex> lock(tasks_mutex);
            tasks.push(value); // 在锁保护下修改队列
        }                     // 先解锁，避免唤醒后消费者马上又阻塞
        tasks_cv.notify_one();
    }

    {
        std::lock_guard<std::mutex> lock(tasks_mutex);
        stopped = true; // 只表示不再生产，队列里的旧任务仍要消费完
    }
    tasks_cv.notify_all();
}

void consumer(int id) {
    while (true) {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        tasks_cv.wait(lock, [] {
            return stopped || !tasks.empty();
        });

        if (stopped && tasks.empty()) {
            return; // 没有新任务且队列已清空，安全退出
        }

        int value = tasks.front();
        tasks.pop();
        lock.unlock(); // 耗时的处理过程不要占着队列锁

        std::cout << "consumer " << id << " 处理任务 " << value << '\n';
    }
}

int main() {
    std::thread worker1(consumer, 1);
    std::thread worker2(consumer, 2);
    std::thread source(producer);

    source.join();
    worker1.join();
    worker2.join();
}
```

### 6.1 wait 内部做了什么

`wait(lock)` 在阻塞前原子地释放互斥锁并进入等待，被唤醒后重新获得锁再返回。这样生产者才有机会拿到同一把锁修改条件。

### 6.2 为什么必须带谓词

```cpp
cv.wait(lock, predicate);
```

它等价于“条件不满足就继续等待”的循环。既能防止**虚假唤醒**后直接往下跑，也能处理通知发生前条件已经满足的情况。

条件变量本身不保存任务状态，真正的条件是 `tasks.empty()` 和 `stopped`。共享条件必须在同一把互斥锁保护下修改，通知可以在解锁后进行。

---

## 七、std::atomic：不可分割的原子操作

对简单计数器，`std::atomic<int>` 能把读—改—写合成一个原子操作，避免互斥锁串行化整个临界区。

```cpp
// atomic_counter.cpp
// 编译：g++ -std=c++17 -Wall -pthread atomic_counter.cpp -o atomic_counter
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    std::atomic<int> counter{0};
    std::vector<std::thread> workers;

    for (int id = 0; id < 4; ++id) {
        workers.emplace_back([&counter] {
            for (int i = 0; i < 10000; ++i) {
                // 这里只需要计数，不发布其他数据，relaxed 已足够。
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    std::cout << counter.load(std::memory_order_relaxed) << '\n'; // 40000
}
```

### 7.1 atomic 不等于“整个对象都线程安全”

```cpp
std::atomic<int> a{0};
std::atomic<int> b{0};

// 两次操作各自是原子的，但组合起来不是一个原子事务。
int old_a = a.load();
if (old_a == 0) {
    b.store(1); // 另一线程可能已经在判断和写入之间修改了 a
}
```

同理，`std::atomic<std::shared_ptr<T>>` 保护的是智能指针对象的并发访问，不代表它指向的 `T` 自动线程安全。

### 7.2 原子也不等于无锁

`std::atomic` 描述的是操作语义，某些类型和平台可能通过内部锁实现。可以用 `is_lock_free()` 做运行期检查；即使无锁，也不代表一定无等待或性能更好。

---

## 八、内存序：原子性之外还有执行顺序

只保证一个变量不被写坏，有时还不够。生产者通常想表达：“先把数据准备好，再设置 ready；消费者看到 ready 后，必须看到完整数据。”这就是内存序要解决的问题。

```cpp
// acquire_release.cpp
// 编译：g++ -std=c++17 -Wall -pthread acquire_release.cpp -o acquire_release
#include <atomic>
#include <cassert>
#include <thread>

int data = 0;
std::atomic<bool> ready{false};

void producer() {
    data = 42; // 1. 先准备普通数据
    ready.store(true, std::memory_order_release); // 2. 发布数据
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {
        std::this_thread::yield(); // 等待发布标志
    }
    assert(data == 42); // 获取操作读到发布后，前面的数据写入可见
}

int main() {
    std::thread consumer_thread(consumer);
    std::thread producer_thread(producer);
    producer_thread.join();
    consumer_thread.join();
}
```

release store 与读取到该值的 acquire load 在同一原子变量上形成同步关系，使生产者发布前的写入对消费者可见。只把 `ready` 换成 atomic，却仍全部使用 relaxed，不能提供这份数据发布保证。

| 内存序 | 通俗理解 | 常见用途 |
|---|---|---|
| `memory_order_relaxed` | 只保证原子操作本身，不同步其他数据 | 独立计数器、统计值 |
| `memory_order_release` | 发布前的写入不能搬到发布之后 | 设置数据就绪标志 |
| `memory_order_acquire` | 获取后的读写不能搬到获取之前 | 读取数据就绪标志 |
| `memory_order_acq_rel` | 一次读—改—写同时具有获取和释放语义 | 需要双向同步的 RMW 操作 |
| `memory_order_seq_cst` | 默认最严格，顺序一致操作存在单一总序 | 默认选择、先保证正确性 |

内存序是并发里最容易“碰巧能跑”的部分。没有充分理由时先用默认 `seq_cst`；改成更弱序前，应能说清同步关系、性能收益和正确性证明。

---

## 九、工程实践：线程不是越多越好

| 问题 | 常见表现 | 改进方向 |
|---|---|---|
| 线程过多 | 上下文切换频繁、内存占用上升 | 使用有界线程池，按任务类型控制并发度 |
| 临界区过大 | 多核跑成单核 | 缩小锁范围，读写分离，减少共享状态 |
| 锁粒度过碎 | 锁竞争和原子操作成本增加 | 批量处理、线程本地聚合后合并 |
| 伪共享 | 不同变量落在同一缓存行，核心互相刷新 | 调整数据布局，把热变量隔离 |
| 任务逃逸 | 子线程访问已销毁的局部变量 | 明确所有权，优先 join 和 RAII |

**伪共享**不是两个线程修改同一个变量，而是它们修改不同变量，却因为变量共用缓存行，导致缓存一致性流量放大。它属于性能问题，不是数据竞争。

C++20 还提供了 `std::jthread`：析构时可自动请求停止并 join，并支持 `stop_token`。但本章示例统一使用 C++17，实际项目是否采用 jthread，要看编译器和项目标准。

---

## 十、面试真题

### 真题 1：join 和 detach 有什么区别？

三句话答案：join 会等待目标线程结束，并在完成后建立同步关系。detach 只分离线程句柄，目标线程继续独立运行，调用方无法再通过原对象等待它。两者只能合理处理一次，否则线程对象在 joinable 状态析构会 terminate。

### 真题 2：为什么 counter++ 不是线程安全的？

三句话答案：普通自增逻辑上包含读取、修改和写回，多个线程可能交叉执行。即使某些平台能发出单条机器指令，也不能据此推出 C++ 层面没有数据竞争。应使用互斥锁或合适的原子操作保护。

### 真题 3：lock_guard 和 unique_lock 怎么选？

三句话答案：lock_guard 是轻量的作用域锁，构造加锁、析构解锁。unique_lock 支持延迟加锁、主动解锁和所有权移动，灵活但管理成本更高。普通临界区优先 lock_guard，条件变量通常需要 unique_lock。

### 真题 4：条件变量为什么必须配合互斥锁？

三句话答案：等待线程需要检查共享条件，通知线程需要修改共享条件，这些访问必须同步。wait 阻塞时释放锁，返回前重新获得锁，让另一方能够修改条件。配合谓词还能处理虚假唤醒和提前发生的通知。

### 真题 5：什么是虚假唤醒？

三句话答案：线程可能在没有收到对应 notify 的情况下从条件变量等待中返回。因而不能假设一次醒来就代表业务条件成立。正确做法是使用 `wait(lock, predicate)` 或循环重新检查条件。

### 真题 6：atomic 和 mutex 怎么选？

三句话答案：简单标志、计数和读—改—写可优先考虑 atomic。mutex 更适合保护多个变量之间的一致性或较长临界区。atomic 不自动保证一组操作的复合原子性，也不一定比锁快。

### 真题 7：volatile 能保证线程安全吗？

三句话答案：不能，volatile 不提供原子性、互斥或 happens-before 关系。它也不保证一个线程的修改按业务需要同步给另一个线程。线程同步应使用 mutex、atomic、条件变量等正式机制。

### 真题 8：release 和 acquire 解决了什么问题？

三句话答案：它们解决的是数据发布时的顺序和可见性，不只是单个变量的原子性。生产者 release 写入就绪标志，消费者 acquire 读到对应值后，可以看到生产者发布前的数据写入。两者必须围绕同一个原子变量形成有效同步关系。

### 真题 9：如何减少死锁？

三句话答案：统一锁顺序、缩短临界区，并避免持锁调用未知外部代码。需要同时获得多把锁时，可使用 `std::scoped_lock` 或 `std::lock`。从设计上减少共享可变状态，往往比事后排查更有效。

### 真题 10：无锁一定比有锁快吗？

三句话答案：不一定，无锁算法可能产生大量 CAS 重试、缓存竞争和内存序成本。atomic 在某些平台也可能并非真正无锁实现。应结合竞争程度、临界区大小和基准测试判断，而不是只看名称。

---

## 十一、踩坑提醒

1. **线程入口函数抛出未捕获异常**：会导致 `std::terminate`，需要用 promise/future 等机制传递异常或在线程内部处理。
2. **按值传参却忘了 std::ref**：线程函数拿到的是参数副本，不会修改调用方原对象。
3. **detach 后引用局部变量**：局部变量销毁后线程仍运行，会造成悬空访问。
4. **只给写操作加锁**：读线程也要遵守同一同步协议。
5. **条件变量只写 `if` 不写谓词/循环**：虚假唤醒后可能处理并不存在的数据。
6. **通知前忘记修改共享条件**：notify 不是任务本身，谓词没有变化时等待方仍应继续等。
7. **把多个原子操作当成一个事务**：每个操作原子，不代表组合逻辑原子。
8. **为了优化随便改 relaxed**：可能破坏数据发布关系，应先证明不需要同步其他数据。
9. **持锁执行慢 IO 或耗时计算**：会放大锁竞争，应在锁内只维护必要的一致性。
10. **运行几次结果正确就认为线程安全**：数据竞争具有偶发性，正确性必须来自同步设计。

---

## 参考资料

本章为原创整理，示例代码为独立编写；仅把以下公开资料用于核对术语、规则与高频考点，未复制原文：

- cppreference 中文：std::thread：https://cppreference.cn/w/cpp/thread/thread
- cppreference 中文：std::mutex：https://cppreference.cn/w/cpp/thread/mutex
- cppreference 中文：std::condition_variable：https://cppreference.cn/w/cpp/thread/condition_variable
- cppreference 中文：std::atomic：https://cppreference.cn/w/cpp/atomic/atomic
- cppreference 中文：std::memory_order：https://cppreference.cn/w/cpp/atomic/memory_order

---

## 上一节 | 下一节

[3.4 Lambda 与函数对象](../lambda/README.md) | [4.1 Linux 基础与排障命令](../../04-linux/01-basics/README.md)

---

> 本教程由**青松与桑叶**编写，面向 C/C++ 面试准备的保姆级中文教学。
