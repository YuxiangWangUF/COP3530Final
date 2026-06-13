// SPDX-License-Identifier: MIT
// Heap benchmark: our MinHeap vs std::priority_queue on identical workloads.
//
// Workloads:
//   1. Random insert + drain
//   2. Ascending insert + drain (worst case for sift-up/down)
//   3. Descending insert + drain
//   4. Top-K pattern over a 1M element feed

#include "imdb/MinHeap.h"

#include <benchmark/benchmark.h>

#include <algorithm>
#include <functional>
#include <numeric>
#include <queue>
#include <random>
#include <vector>

namespace {

std::vector<int> MakeRandom(std::size_t n, uint32_t seed = 42) {
    std::mt19937 rng(seed);
    std::vector<int> v(n);
    for (auto& x : v) x = static_cast<int>(rng());
    return v;
}

std::vector<int> MakeAscending(std::size_t n) {
    std::vector<int> v(n);
    std::iota(v.begin(), v.end(), 0);
    return v;
}

std::vector<int> MakeDescending(std::size_t n) {
    std::vector<int> v(n);
    std::iota(v.rbegin(), v.rend(), 0);
    return v;
}

}  // namespace

// ---------------- Random insert + drain ----------------

static void BM_StdPQ_Random(benchmark::State& state) {
    auto data = MakeRandom(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        std::priority_queue<int> pq(std::less<int>{}, data);
        // Drain.
        while (!pq.empty()) {
            benchmark::DoNotOptimize(pq.top());
            pq.pop();
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdPQ_Random)->Range(1 << 10, 1 << 20);

static void BM_OurHeap_Random(benchmark::State& state) {
    auto data = MakeRandom(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        imdb::MinHeap<int> h;
        for (int x : data) h.push(x);
        while (!h.empty()) {
            benchmark::DoNotOptimize(h.top());
            h.pop();
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_OurHeap_Random)->Range(1 << 10, 1 << 20);

// ---------------- Ascending (worst case) ----------------

static void BM_StdPQ_Ascending(benchmark::State& state) {
    auto data = MakeAscending(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        std::priority_queue<int, std::vector<int>, std::greater<int>> pq(
            std::greater<int>{}, data);
        while (!pq.empty()) {
            benchmark::DoNotOptimize(pq.top());
            pq.pop();
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_StdPQ_Ascending)->Range(1 << 10, 1 << 20);

static void BM_OurHeap_Ascending(benchmark::State& state) {
    auto data = MakeAscending(static_cast<std::size_t>(state.range(0)));
    for (auto _ : state) {
        imdb::MinHeap<int, std::greater<int>> h;
        for (int x : data) h.push(x);
        while (!h.empty()) {
            benchmark::DoNotOptimize(h.top());
            h.pop();
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_OurHeap_Ascending)->Range(1 << 10, 1 << 20);

// ---------------- Top-K pattern over 1M ----------------
// Keeps only the K=10 smallest of a 1M-element random stream.

static void BM_TopK_StdPQ(benchmark::State& state) {
    constexpr std::size_t N = 1u << 20;
    constexpr std::size_t K = 10;
    auto data = MakeRandom(N);
    for (auto _ : state) {
        // max-heap (greater) so the top is the largest of the K smallest
        std::priority_queue<int> topk(std::less<int>{});
        for (int x : data) {
            if (topk.size() < K) {
                topk.push(x);
            } else if (x < topk.top()) {
                topk.pop();
                topk.push(x);
            }
        }
        benchmark::DoNotOptimize(topk.top());
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_TopK_StdPQ);

static void BM_TopK_OurHeap(benchmark::State& state) {
    constexpr std::size_t N = 1u << 20;
    constexpr std::size_t K = 10;
    auto data = MakeRandom(N);
    for (auto _ : state) {
        imdb::MinHeap<int, std::greater<int>> topk;
        topk.reserve(K);
        for (int x : data) {
            if (topk.size() < K) {
                topk.push(x);
            } else if (x < topk.top()) {
                topk.pop();
                topk.push(x);
            }
        }
        benchmark::DoNotOptimize(topk.top());
    }
    state.SetItemsProcessed(state.iterations() * N);
}
BENCHMARK(BM_TopK_OurHeap);

BENCHMARK_MAIN();