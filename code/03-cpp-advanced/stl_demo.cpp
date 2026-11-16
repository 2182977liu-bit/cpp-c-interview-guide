/**
 * @file stl_demo.cpp
 * @brief 3.1 STL 容器与算法 —— 完整示例代码
 * @author 青松与桑叶
 *
 * 编译: g++ -std=c++17 -Wall -o stl_demo stl_demo.cpp
 * 运行: ./stl_demo
 *
 * 说明: 本文件用可观察的输出，把"扩容倍率、迭代器失效、rehash、就地构造"
 *       这些面试常问但看不到的现象打印出来。
 */

#include <algorithm>
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;

// ============================================================
// 辅助类：带编号的构造/析构/移动，用来观察"搬运次数"
// ============================================================

class Tracked {
public:
    int id;
    static int ctor_count;
    static int move_count;
    static int copy_count;

    explicit Tracked(int i = 0) : id(i) {
        ++ctor_count;
        cout << "    构造 #" << id << endl;
    }

    // 拷贝构造
    Tracked(const Tracked& o) : id(o.id) {
        ++copy_count;
        cout << "    拷贝构造 #" << id << endl;
    }

    // 移动构造：声明 noexcept 后，vector 扩容才会用它
    Tracked(Tracked&& o) noexcept : id(o.id) {
        o.id = -1;
        ++move_count;
        cout << "    移动构造 #" << id << endl;
    }

    ~Tracked() {
        if (id >= 0) cout << "    析构 #" << id << endl;
    }
};
int Tracked::ctor_count = 0;
int Tracked::move_count = 0;
int Tracked::copy_count = 0;

// ============================================================
// 第一部分：vector 的扩容倍率
// ============================================================

void demo_vector_growth() {
    cout << "\n========== 1. vector 扩容倍率观察 ==========" << endl;

    vector<int> v;
    size_t last_cap = 0;
    for (int i = 0; i < 33; ++i) {
        if (v.capacity() != last_cap) {
            cout << "  push 第 " << i + 1 << " 个元素后: size=" << v.size()
                 << " capacity=" << v.capacity();
            if (last_cap > 0) {
                cout << "  (本次扩容倍数 " << (double)v.capacity() / last_cap << ")";
            }
            cout << endl;
            last_cap = v.capacity();
        }
        v.push_back(i);
    }
    cout << "  结论: libstdc++ 按 2 倍增长，MSVC 按 1.5 倍增长" << endl;
    cout << "  首元素地址是否连续: " << (v.data() == &v[0] ? "是（连续内存）" : "否") << endl;
}

// ============================================================
// 第二部分：reserve 与 resize
// ============================================================

void demo_reserve_resize() {
    cout << "\n========== 2. reserve 与 resize ==========" << endl;

    vector<int> a;
    a.reserve(10);
    cout << "  reserve(10) 后: size=" << a.size() << " capacity=" << a.capacity()
         << "  （只改容量，不构造元素）" << endl;

    vector<int> b;
    b.resize(10);
    cout << "  resize(10)  后: size=" << b.size() << " capacity=" << b.capacity()
         << "  （真的构造了 10 个元素）" << endl;

    // shrink_to_fit 只是"请求"，标准不保证一定释放
    vector<int> c;
    c.reserve(1000);
    c.push_back(1);
    c.shrink_to_fit();
    cout << "  shrink_to_fit 后: capacity=" << c.capacity()
         << "（实现通常缩到 size，但不保证）" << endl;
}

// ============================================================
// 第三部分：扩容时是拷贝还是移动
// ============================================================

void demo_realloc_copy_or_move() {
    cout << "\n========== 3. 扩容时元素如何搬运 ==========" << endl;

    cout << "  [3.1] 元素有 noexcept 移动构造 → 用移动" << endl;
    {
        Tracked::ctor_count = Tracked::move_count = Tracked::copy_count = 0;
        vector<Tracked> v;
        for (int i = 0; i < 5; ++i) v.emplace_back(i);
        cout << "  统计: 构造=" << Tracked::ctor_count
             << " 移动=" << Tracked::move_count
             << " 拷贝=" << Tracked::copy_count << endl;
    }

    cout << "  [3.2] 先 reserve 到足够大小 → 完全不搬" << endl;
    {
        Tracked::ctor_count = Tracked::move_count = Tracked::copy_count = 0;
        vector<Tracked> v;
        v.reserve(8);
        for (int i = 0; i < 5; ++i) v.emplace_back(i);
        cout << "  统计: 构造=" << Tracked::ctor_count
             << " 移动=" << Tracked::move_count
             << " 拷贝=" << Tracked::copy_count << endl;
    }
    cout << "  结论: 已知规模时 reserve 能省掉全部搬运" << endl;
}

// ============================================================
// 第四部分：迭代器失效
// ============================================================

void demo_iterator_invalidation() {
    cout << "\n========== 4. 迭代器失效 ==========" << endl;

    // 4.1 保存的迭代器在扩容后悬空
    vector<int> v{1, 2, 3};
    auto it = v.begin();
    cout << "  扩容前 *it = " << *it << endl;
    // 强制触发扩容，让旧迭代器指向已释放内存
    v.reserve(v.capacity() * 2 + 1);
    cout << "  reserve 触发重新分配后，旧迭代器是否仍指向原地址: "
         << (it == v.begin() ? "是" : "否（旧迭代器已失效，解引用即 UB）") << endl;

    // 4.2 正确的删除写法：用 erase 的返回值接住下一个迭代器
    vector<int> nums{1, 2, 3, 4, 5, 2, 6};
    for (auto cur = nums.begin(); cur != nums.end();) {
        if (*cur == 2) cur = nums.erase(cur);
        else ++cur;
    }
    cout << "  删除所有 2 之后: ";
    for (int x : nums) cout << x << " ";
    cout << endl;

    // 4.3 list 删除当前元素不影响其他迭代器
    list<int> lst{1, 2, 3, 4};
    auto lit = lst.begin();
    ++lit;                       // 指向 2
    lst.erase(lit);              // 只让 lit 失效
    cout << "  list 中删除 2 后首元素: " << lst.front()
         << "（其余迭代器不受影响）" << endl;
}

// ============================================================
// 第五部分：deque 与 list 的特性
// ============================================================

void demo_deque_list() {
    cout << "\n========== 5. deque 与 list ==========" << endl;

    deque<int> dq{2, 3};
    dq.push_front(1);            // O(1)：在首块里往前写
    dq.push_back(4);             // O(1)：在尾块里往后写
    cout << "  deque 双端插入后: ";
    for (int x : dq) cout << x << " ";
    cout << endl;
    cout << "  deque 支持随机访问 dq[2] = " << dq[2] << "（两次间接寻址）" << endl;

    list<int> lst{1, 3};
    auto pos = lst.begin();
    ++pos;                       // 指向 3
    lst.insert(pos, 2);          // O(1) 插入，但找 pos 是 O(n)
    cout << "  list 中间插入 2 后: ";
    for (int x : lst) cout << x << " ";
    cout << endl;
    cout << "  list 不支持随机访问，只能逐个 ++" << endl;
}

// ============================================================
// 第六部分：map 与 unordered_map
// ============================================================

void demo_map_vs_unordered() {
    cout << "\n========== 6. map 与 unordered_map ==========" << endl;

    map<string, int> ordered;
    ordered["banana"] = 2;
    ordered["apple"] = 1;
    ordered["cherry"] = 3;
    cout << "  map 遍历顺序（按 key 排序）: ";
    for (auto& kv : ordered) cout << kv.first << " ";
    cout << endl;
    cout << "  map 支持范围查询 lower_bound(\"b\") -> "
         << ordered.lower_bound("b")->first << endl;

    unordered_map<string, int> hashed;
    hashed["banana"] = 2;
    hashed["apple"] = 1;
    hashed["cherry"] = 3;
    cout << "  unordered_map 遍历顺序（依赖哈希，不保证）: ";
    for (auto& kv : hashed) cout << kv.first << " ";
    cout << endl;
}

// ============================================================
// 第七部分：rehash 的触发与代价
// ============================================================

void demo_rehash() {
    cout << "\n========== 7. unordered_map 的 rehash ==========" << endl;

    unordered_map<int, int> m;
    cout << "  max_load_factor = " << m.max_load_factor() << endl;

    size_t last_buckets = m.bucket_count();
    cout << "  初始 bucket_count = " << last_buckets << endl;

    size_t rehash_times = 0;
    for (int i = 0; i < 200; ++i) {
        m[i] = i;
        if (m.bucket_count() != last_buckets) {
            ++rehash_times;
            cout << "    第 " << i + 1 << " 次插入触发 rehash: buckets "
                 << last_buckets << " -> " << m.bucket_count()
                 << "  load_factor=" << m.load_factor() << endl;
            last_buckets = m.bucket_count();
        }
    }
    cout << "  共触发 rehash " << rehash_times << " 次；reserve 可避免" << endl;

    // 预留桶数后再次插入，不再 rehash
    unordered_map<int, int> pre;
    pre.reserve(200);
    size_t buckets_before = pre.bucket_count();
    for (int i = 0; i < 200; ++i) pre[i] = i;
    cout << "  reserve(200) 后 bucket_count = " << buckets_before
         << "，插入 200 个元素后 = " << pre.bucket_count() << endl;
}

// ============================================================
// 第八部分：push_back 与 emplace_back
// ============================================================

void demo_push_vs_emplace() {
    cout << "\n========== 8. push_back 与 emplace_back ==========" << endl;

    cout << "  [8.1] push_back 传入临时对象" << endl;
    {
        vector<string> v;
        v.push_back(string("hello"));     // 构造临时对象 + 移动进容器
        cout << "    size=" << v.size() << " 内容=" << v[0] << endl;
    }

    cout << "  [8.2] emplace_back 直接就地构造" << endl;
    {
        vector<string> v;
        v.emplace_back("hello");          // 直接在容器内存上构造
        cout << "    size=" << v.size() << " 内容=" << v[0] << endl;
    }

    cout << "  [8.3] 构造 pair 时 emplace_back 优势明显" << endl;
    {
        vector<pair<string, int>> v;
        v.push_back(make_pair("a", 1));   // 构造 pair + 移动
        v.emplace_back("b", 2);           // 两个参数直接构造 pair
        cout << "    " << v[0].first << "=" << v[0].second
             << ", " << v[1].first << "=" << v[1].second << endl;
    }
}

// ============================================================
// 第九部分：算法与迭代器解耦
// ============================================================

void demo_algorithms() {
    cout << "\n========== 9. 算法只认迭代器，不认容器 ==========" << endl;

    vector<int> v{5, 2, 8, 1, 9};
    sort(v.begin(), v.end());                 // vector 用随机访问迭代器
    cout << "  vector 排序后: ";
    for (int x : v) cout << x << " ";
    cout << endl;

    list<int> lst{5, 2, 8, 1, 9};
    // list 的迭代器不是随机访问，不能用 std::sort，要用成员函数
    lst.sort();
    cout << "  list 排序后: ";
    for (int x : lst) cout << x << " ";
    cout << endl;

    auto pos = find(v.begin(), v.end(), 8);
    cout << "  find 在 vector 里找 8: "
         << (pos != v.end() ? "找到" : "未找到") << endl;

    // 算法 + 仿函数：按绝对值排序
    vector<int> nums{3, -1, 2, -5};
    sort(nums.begin(), nums.end(), [](int a, int b) {
        return std::abs(a) < std::abs(b);
    });
    cout << "  按绝对值排序: ";
    for (int x : nums) cout << x << " ";
    cout << endl;
}

// ============================================================

int main() {
    cout << "===== 3.1 STL 容器与算法 演示 =====" << endl;

    demo_vector_growth();
    demo_reserve_resize();
    demo_realloc_copy_or_move();
    demo_iterator_invalidation();
    demo_deque_list();
    demo_map_vs_unordered();
    demo_rehash();
    demo_push_vs_emplace();
    demo_algorithms();

    cout << "\n===== 演示结束 =====" << endl;
    return 0;
}
