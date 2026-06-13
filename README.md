# IMDb Top-250 Recommender

[![CI](https://github.com/YuxiangWangUF/IMDb-Top250-Recommender/actions/workflows/ci.yml/badge.svg)](https://github.com/YuxiangWangUF/IMDb-Top250-Recommender/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
![Language](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Platforms](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows-lightgrey.svg)
![Build](https://img.shields.io/badge/build-cmake-success)

> A production-grade C++17 library that ranks IMDb titles in **O(N log K)**
> using a hand-rolled templated `MinHeap` and `RedBlackTree` — benchmarked
> head-to-head against `std::priority_queue` and `std::map`.

This repository is the modernized version of a 3-year-old COP 3530
(Florida, UF) course project. The legacy code is preserved under
[`old_code/`](old_code/) for reference.

---

## Points

1. **Algorithm depth.** Two textbook data structures implemented from
   scratch in modern C++17 — left-leaning red-black tree with sentinel
   NIL, and a binary min-heap templated on a strict weak ordering.
2. **Real data, real constraints.** Streams `title.basics.tsv` (GB-scale)
   with bounded memory: **O(K)** for Top-K, never `O(N)`.
3. **Production-grade engineering.** `-Wall -Wextra -Wpedantic -Werror`
   on GCC, Clang, and MSVC; GoogleTest suite (80+ cases); Google
   Benchmark comparison vs the standard library; CMake `FetchContent`
   for zero-setup reproducibility; CI across Linux + Windows.
4. **Honest measurements.** We benchmarked the homegrown implementations
   against `std::priority_queue` / `std::map` on identical workloads. The
   results are published in this README — no cherry-picking.

---

## Quick start

```bash
# Configure + build
cmake -B build -S . -DIMDB_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# Run unit tests (81 cases)
ctest --test-dir build --output-on-failure -j1

# Run the CLI on the bundled sample (Top-5 movies, rating >= 7.5)
./build/imdb-topk --data data/sample.tsv --top 5
```

On Windows + MSVC, the binary lands in `build\Release\imdb-topk.exe`;
adjust the path above accordingly.

---

## CLI usage

```
imdb-topk --data <path.tsv> [--top N] [--min-votes V] [--min-rating R]

  --data PATH        Input TSV path (required unless --help)
  --top N            Show top-N movies.        [default: 10]
  --min-votes V      Minimum votes cutoff.     [default: 10000]
  --min-rating R     Minimum rating cutoff.    [default: 7.5]
  --help, -h         Print this help
```

Example output (with the bundled 7-row `data/sample.tsv`):

```
rank    title                       year  rating  votes
1       The Shawshank Redemption    1994  9.3     2500000
2       The Godfather               1972  9.2     1900000
3       The Dark Knight             2008  9.0     2700000
4       Movie, with Comma           1997  8.5     500000
5       测试中文 UTF-8 标题           2024  8.7     300000
```

---

## Performance

All numbers from the bundled `bench_heap` / `bench_rbtree` targets,
MSVC 19.50 / x64 / `Release`. Each cell is the median CPU time across
the default iteration count; lower is better.

### Heap vs `std::priority_queue`

| Workload                  | N        | `std::priority_queue` | `imdb::MinHeap` |
|---------------------------|----------|----------------------|-----------------|
| Random insert + drain     | 1,048,576 | 75 ms                | 141 ms          |
| Ascending insert + drain  | 1,048,576 | 47 ms                | 80 ms           |
| Top-K (K=10) over 1M feed | 1,048,576 | 0.78 ms              | 0.78 ms         |

`std::priority_queue` is consistently ~2x faster on the trivial
workloads because libstdc++/MSVC hand-tune `std::vector`-backed heaps
with strong inlining. For the **Top-K** pattern that matters in
practice, our heap matches it exactly: same data layout, same sift
strategy, same wall time. The point of the benchmark is to prove we
didn't lose — not to claim victory over a decade of standard-library
optimization.

### RedBlackTree vs `std::map`

| Workload                  | N        | `std::map` | `imdb::RedBlackTree` |
|---------------------------|----------|------------|----------------------|
| Insert-only (random)      | 1,048,576 | 83 ms     | 125 ms               |
| Lookup (random probes)    | 1,048,576 | 625 ms    | **500 ms** (-20%)    |
| Ascending insert          | 1,048,576 | 130 ms    | **125 ms**           |
| Insert + 50% erase        | 1,048,576 | 203 ms    | **~150 ms** (-26%)   |

Our lookup and mixed workloads are 20–26% faster than
`std::map` because the standard library's `std::map` is
header-only with type-erased comparators and color stored in the
parent pointer's high bit — both slow paths. Our class is simpler
and the cache behavior is friendlier.

---

## Architecture

```
include/imdb/
├── MinHeap.h          -- templated binary min-heap
├── RedBlackTree.h     -- templated red-black tree + bidirectional iterator
├── Movie.h            -- IMDb title.basics.tsv row + strict total order
├── Loader.h           -- streaming TSV loader (LoadMovies / StreamMovies)
└── TopK.h             -- bounded-heap Top-K core (header-only)
src/
├── loader.cpp         -- TSV parser, BOM/CRLF tolerant, quoted-field aware
└── cli.cpp            -- imdb-topk CLI driver
tests/
├── test_loader.cpp    -- 23 cases: empty, header, single-row, filters, escapes,
│                                   BOM/CRLF, streaming, sample fixture
├── test_minheap.cpp   -- 22 cases: empty/single/random/duplicate/Top-K pattern
├── test_rbtree.cpp    -- 30 cases: every erase shape, bounds, invariants
└── test_cli.cpp       --  6 cases: top_k core + CLI integration
benchmarks/
├── bench_heap.cpp     -- vs std::priority_queue (4 workloads, 3 sizes)
└── bench_rbtree.cpp   -- vs std::map (5 workloads, 3 sizes)
data/
└── sample.tsv         -- 7-row fixture, covers edge cases (BOM, CRLF,
                          quoted fields, UTF-8 title, comma-in-title, \N)
```

---

---

## License

MIT — see [LICENSE](LICENSE).

---

# IMDb Top-250 推荐器（中文）

> 一个生产级 C++17 库,用**手写的模板化 MinHeap 和 RedBlackTree** 在
> **O(N log K)** 时间复杂度内对 IMDb 影片排序,与 `std::priority_queue` /
> `std::map` 同台对标。

## 亮点

1. **算法深度。** 两个教科书级数据结构用现代 C++17 从零实现——左倾
   红黑树带哨兵 NIL 节点,二叉小顶堆模板化在严格弱序上。
2. **真实数据,真实约束。** 流式处理 `title.basics.tsv`(GB 级),内存
   占用恒定 **O(K)**(Top-K),永远不是 O(N)。
3. **生产级工程质量。** GCC / Clang / MSVC 三家编译器全开 `-Wall
   -Wextra -Wpedantic -Werror`;GoogleTest 80+ 用例;Google Benchmark
   与标准库对比;CMake `FetchContent` 零配置复现;Linux + Windows CI。
4. **诚实的数据。** 我们跑了自己的实现和标准库同负载基准,结果公开在
   这里,不挑选。

## 快速上手

```bash
cmake -B build -S . -DIMDB_BUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build --output-on-failure -j1
./build/imdb-topk --data data/sample.tsv --top 5
```

## 性能(MSVC 19.50 / x64 / Release)

### Heap vs `std::priority_queue`

| 负载            | N        | `std::priority_queue` | `imdb::MinHeap` |
|----------------|----------|----------------------|-----------------|
| 随机插入+排空   | 1,048,576 | 75 ms               | 141 ms          |
| 顺序插入+排空   | 1,048,576 | 47 ms               | 80 ms           |
| Top-K(K=10)     | 1,048,576 | 0.78 ms             | **0.78 ms**     |

### RedBlackTree vs `std::map`

| 负载            | N        | `std::map` | `imdb::RedBlackTree` |
|----------------|----------|------------|----------------------|
| 查找            | 1,048,576 | 625 ms    | **500 ms** (-20%)    |
| 插入 + 50% 删除 | 1,048,576 | 203 ms    | **~150 ms** (-26%)   |

我们的查找和混合负载比 `std::map` 快 20–26%。`std::map` 因为是头文件
库 + 类型擦除比较器 + 把颜色存在父指针的高位里,在这些路径上反而慢。

## 许可证

MIT——见 [LICENSE](LICENSE)。
