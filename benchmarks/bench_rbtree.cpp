// SPDX-License-Identifier: MIT
// RedBlackTree benchmark: ours vs std::map on identical workloads.
//
// Workloads:
//   1. Random insert-only
//   2. Random insert + lookup
//   3. Ascending insert (sequential worst case for insertion)
//   4. Random insert + erase (50/50 mix)
//   5. Iteration over a fully built tree

#include "imdb/RedBlackTree.h"

#include <benchmark/benchmark.h>

#include <map>
#include <random>
#include <vector>

namespace {

std::vector<int> MakeRandom(std::size_t n, uint32_t seed = 42) {
    std::mt19937 rng(seed);
    std::vector<int> v(n);
    for (auto& x : v) x = static_cast<int>(rng());
    return v;
}

std::vector<int> MakeUniqueRandom(std::size_t n, uint32_t seed = 42) {
    std::mt19937 rng(seed);
    std::vector<int> v(n);
    for (auto& x : v) x = static_cast<int>(rng());
    // Deduplicate (RBT / map are unique-key containers).
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    return v;
}

}  // namespace

// ---------------- Random insert-only ----------------

static void BM_StdMap_InsertOnly(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        std::map<int, int> m;
        for (int x : data) m.emplace(x, x);
        benchmark::DoNotOptimize(m.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdMap_InsertOnly)->Range(1 << 10, 1 << 20);

static void BM_OurRBT_InsertOnly(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        imdb::RedBlackTree<int, int> t;
        for (int x : data) t.insert(x, x);
        benchmark::DoNotOptimize(t.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_OurRBT_InsertOnly)->Range(1 << 10, 1 << 20);

// ---------------- Random insert + lookup ----------------

static void BM_StdMap_Lookup(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    std::map<int, int> m;
    for (int x : data) m.emplace(x, x);
    auto probes = MakeRandom(static_cast<std::size_t>(state.range(0)), 7);
    for (auto _ : state) {
        std::size_t hits = 0;
        for (int x : probes) {
            auto it = m.find(x);
            if (it != m.end()) ++hits;
        }
        benchmark::DoNotOptimize(hits);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdMap_Lookup)->Range(1 << 10, 1 << 20);

static void BM_OurRBT_Lookup(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    imdb::RedBlackTree<int, int> t;
    for (int x : data) t.insert(x, x);
    auto probes = MakeRandom(static_cast<std::size_t>(state.range(0)), 7);
    for (auto _ : state) {
        std::size_t hits = 0;
        for (int x : probes) {
            if (t.contains(x)) ++hits;
        }
        benchmark::DoNotOptimize(hits);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_OurRBT_Lookup)->Range(1 << 10, 1 << 20);

// ---------------- Ascending insert ----------------

static void BM_StdMap_Ascending(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        std::map<int, int> m;
        for (std::size_t i = 0; i < n; ++i) m.emplace(static_cast<int>(i), 0);
        benchmark::DoNotOptimize(m.size());
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_StdMap_Ascending)->Range(1 << 10, 1 << 20);

static void BM_OurRBT_Ascending(benchmark::State& state) {
    const std::size_t n = static_cast<std::size_t>(state.range(0));
    for (auto _ : state) {
        imdb::RedBlackTree<int, int> t;
        for (std::size_t i = 0; i < n; ++i) t.insert(static_cast<int>(i), 0);
        benchmark::DoNotOptimize(t.size());
    }
    state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_OurRBT_Ascending)->Range(1 << 10, 1 << 20);

// ---------------- Random insert + random erase ----------------

static void BM_StdMap_InsertErase(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    std::mt19937 rng(123);
    for (auto _ : state) {
        std::map<int, int> m;
        for (int x : data) m.emplace(x, x);
        std::size_t i = 0;
        for (int x : data) {
            if ((rng() & 1) && i++ > 0) m.erase(x);
        }
        benchmark::DoNotOptimize(m.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdMap_InsertErase)->Range(1 << 10, 1 << 20);

static void BM_OurRBT_InsertErase(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    std::mt19937 rng(123);
    for (auto _ : state) {
        imdb::RedBlackTree<int, int> t;
        for (int x : data) t.insert(x, x);
        std::size_t i = 0;
        for (int x : data) {
            if ((rng() & 1) && i++ > 0) t.erase(x);
        }
        benchmark::DoNotOptimize(t.size());
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_OurRBT_InsertErase)->Range(1 << 10, 1 << 20);

// ---------------- Full iteration ----------------

static void BM_StdMap_Iterate(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    std::map<int, int> m;
    for (int x : data) m.emplace(x, x);
    for (auto _ : state) {
        std::int64_t sum = 0;
        for (auto& kv : m) sum += kv.first;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdMap_Iterate)->Range(1 << 10, 1 << 20);

static void BM_OurRBT_Iterate(benchmark::State& state) {
    auto data = MakeUniqueRandom(static_cast<std::size_t>(state.range(0)));
    imdb::RedBlackTree<int, int> t;
    for (int x : data) t.insert(x, x);
    for (auto _ : state) {
        std::int64_t sum = 0;
        for (auto& kv : t) sum += kv.first;
        benchmark::DoNotOptimize(sum);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_OurRBT_Iterate)->Range(1 << 10, 1 << 20);

BENCHMARK_MAIN();