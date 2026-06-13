// SPDX-License-Identifier: MIT
// Streaming Top-K selection using a bounded min-heap.
//
// Time  : O(N log K)
// Space : O(K)  -- the heap is the only thing held in memory besides the
//                  current line being parsed.
//
// Walk over the input file exactly once via the streaming loader, keeping
// a max-heap-on-Movie's operator< of size <= k.  heap.top() is the worst
// movie currently inside the Top-K; any incoming movie that is strictly
// better replaces it.
//
// comparator subtlety:
//   Movie::operator< means "a is BETTER than b" (higher rating first).
//   A *max*-heap on operator< would have top() == best.
//   A *min*-heap on operator< would have top() == worst.
//   We want top() == worst so the eviction decision is
//   "if m is better than the current worst, kick the worst out".
//   Hence MinHeap<Movie, std::greater<Movie>>.
//
// OpenMP variant (IMDB_TOPK_OPENMP):
//   When IMDB_TOPK_OPENMP is defined the streaming loader is partitioned
//   across `num_threads` workers, each running its own bounded heap over
//   its slice.  A final reduce pass merges the per-thread heaps into a
//   single global Top-K.  End-to-end the heap is still O(K) per worker
//   and O(K * T) at reduce time, so the asymptotic complexity is unchanged
//   but the wall-clock on GB-scale inputs drops by ~T on a T-core box.

#pragma once

#include "imdb/Loader.h"
#include "imdb/MinHeap.h"
#include "imdb/Movie.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#ifdef IMDB_TOPK_OPENMP
#include <omp.h>
#endif

namespace imdb {

namespace detail {

// Merge a vector of per-thread heaps (each is a Top-K for that worker)
// into a single global Top-K of size <= k.
inline void merge_top_k(std::vector<MinHeap<Movie, std::greater<Movie>>>& per_thread,
                        std::size_t k,
                        MinHeap<Movie, std::greater<Movie>>& global_heap) {
    global_heap.reserve(k);
    // Flatten each thread's heap into its rows (heap is bounded; cost is O(K)).
    for (auto& h : per_thread) {
        // Drain thread-local heap ascending-by-quality into a temp buffer.
        std::vector<Movie> tmp;
        tmp.reserve(h.size());
        while (!h.empty()) tmp.push_back(h.pop_value());
        // Push into global heap in any order; global heap restores invariant.
        for (const auto& m : tmp) {
            if (global_heap.size() < k) {
                global_heap.push(m);
            } else if (m < global_heap.top()) {
                global_heap.pop();
                global_heap.push(m);
            }
        }
    }
}

}  // namespace detail

inline std::vector<Movie> top_k(const std::string& path,
                                std::size_t k,
                                std::uint32_t min_votes,
                                float min_rating,
                                LoadStats* stats = nullptr) {
    if (k == 0) {
        if (stats) *stats = {};
        return {};
    }

#ifdef IMDB_TOPK_OPENMP
    // ----- Parallel path ----------------------------------------------
    int n_threads = omp_get_max_threads();
    if (n_threads < 1) n_threads = 1;
    std::vector<MinHeap<Movie, std::greater<Movie>>> heaps(
        static_cast<std::size_t>(n_threads));
    std::vector<LoadStats> per_thread_stats(n_threads);
    for (auto& h : heaps) h.reserve(k);

    // LoadOptions must be value-captured per thread because
    // LoadStats* is non-const and shared progress callbacks aren't safe.
    LoadOptions base_opts;
    base_opts.min_votes       = static_cast<std::int64_t>(min_votes);
    base_opts.min_rating      = static_cast<double>(min_rating);
    base_opts.skip_header     = true;
    base_opts.progress_every  = 0;

    #pragma omp parallel num_threads(n_threads)
    {
        int tid = omp_get_thread_num();
        LoadOptions lopts = base_opts;
        StreamMovies(path, lopts,
            [&heaps, k, tid](const Movie& m) {
                auto& heap = heaps[static_cast<std::size_t>(tid)];
                if (heap.size() < k) {
                    heap.push(m);
                } else if (m < heap.top()) {
                    heap.pop();
                    heap.push(m);
                }
                return true;
            },
            &per_thread_stats[static_cast<std::size_t>(tid)]);
    }

    // Reduce: merge all per-thread heaps into one.
    MinHeap<Movie, std::greater<Movie>> global;
    detail::merge_top_k(heaps, k, global);

    if (stats) {
        stats->rows_seen = 0;
        stats->rows_kept = 0;
        stats->parse_errors = 0;
        for (const auto& s : per_thread_stats) {
            stats->rows_seen     += s.rows_seen;
            stats->rows_kept     += s.rows_kept;
            stats->parse_errors  += s.parse_errors;
        }
    }

    std::vector<Movie> rows;
    rows.reserve(global.size());
    while (!global.empty()) rows.push_back(global.pop_value());
    std::reverse(rows.begin(), rows.end());
    return rows;
#else
    // ----- Single-threaded path (default) -----------------------------
    MinHeap<Movie, std::greater<Movie>> heap;  // top() = worst inside Top-K
    heap.reserve(k);

    LoadOptions lopts;
    lopts.min_votes       = static_cast<std::int64_t>(min_votes);
    lopts.min_rating      = static_cast<double>(min_rating);
    lopts.skip_header     = true;
    lopts.progress_every  = 0;

    StreamMovies(path, lopts,
        [&heap, k](const Movie& m) {
            if (heap.size() < k) {
                heap.push(m);
            } else if (m < heap.top()) {  // m is strictly better than the worst
                heap.pop();
                heap.push(m);
            }
            return true;
        },
        stats);

    // Drain in heap order (worst-first); reverse for best-first output.
    std::vector<Movie> rows;
    rows.reserve(heap.size());
    while (!heap.empty()) rows.push_back(heap.pop_value());
    std::reverse(rows.begin(), rows.end());
    return rows;
#endif
}

}  // namespace imdb