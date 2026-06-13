// SPDX-License-Identifier: MIT
// MinHeap unit tests -- 17 cases covering empty/single/random/duplicate,
// comparator, reserve, range, move semantics, and clear.

#include "imdb/MinHeap.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

// 1. Default ctor + empty
TEST(MinHeapInt, DefaultCtorIsEmpty) {
    imdb::MinHeap<int> h;
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(h.size(), 0u);
}

// 2. Single element: top is the element itself.
TEST(MinHeapInt, SingleElement) {
    imdb::MinHeap<int> h;
    h.push(42);
    EXPECT_FALSE(h.empty());
    EXPECT_EQ(h.size(), 1u);
    EXPECT_EQ(h.top(), 42);
    h.pop();
    EXPECT_TRUE(h.empty());
}

// 3. Top of random sequence
TEST(MinHeapInt, RandomSequenceTopIsMin) {
    imdb::MinHeap<int> h;
    std::vector<int> v = {7, 3, 9, 1, 4, 8, 2, 6, 5, 0};
    for (int x : v) h.push(x);
    EXPECT_EQ(h.top(), 0);
    EXPECT_EQ(h.size(), v.size());
}

// 4. Repeated pops produce sorted output
TEST(MinHeapInt, PopSequenceIsSorted) {
    imdb::MinHeap<int> h;
    std::vector<int> v = {7, 3, 9, 1, 4, 8, 2, 6, 5, 0};
    for (int x : v) h.push(x);
    std::vector<int> popped;
    while (!h.empty()) popped.push_back(h.pop_value());
    EXPECT_TRUE(std::is_sorted(popped.begin(), popped.end()));
    EXPECT_EQ(popped, (std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}));
}

// 5. Batch push_heap from a range
TEST(MinHeapInt, BatchPushHeapRange) {
    std::vector<int> v = {12, 11, 13, 5, 6, 7};
    imdb::MinHeap<int> h;
    h.push_heap_range(v.begin(), v.end());
    EXPECT_EQ(h.size(), 6u);
    EXPECT_EQ(h.top(), 5);
    std::vector<int> popped;
    while (!h.empty()) popped.push_back(h.pop_value());
    EXPECT_TRUE(std::is_sorted(popped.begin(), popped.end()));
}

// 6. Reserve avoids realloc churn (sanity test: works with large reserve)
TEST(MinHeapInt, Reserve) {
    imdb::MinHeap<int> h;
    h.reserve(10'000);
    EXPECT_GE(h.capacity(), 10'000u);
    for (int i = 0; i < 10'000; ++i) h.push(10'000 - i);
    EXPECT_EQ(h.size(), 10'000u);
    EXPECT_EQ(h.top(), 1);
}

// 7. Reverse-order push: no-op for the heap
TEST(MinHeapInt, ReverseOrderPush) {
    imdb::MinHeap<int> h;
    for (int i = 100; i >= 1; --i) h.push(i);
    EXPECT_EQ(h.top(), 1);
    std::vector<int> popped;
    while (!h.empty()) popped.push_back(h.pop_value());
    EXPECT_TRUE(std::is_sorted(popped.begin(), popped.end()));
}

// 8. Duplicates are stable in count (not in order -- heap doesn't promise that)
TEST(MinHeapInt, Duplicates) {
    imdb::MinHeap<int> h;
    for (int x : {5, 3, 5, 1, 3, 5, 1}) h.push(x);
    int ones = 0, threes = 0, fives = 0;
    while (!h.empty()) {
        int v = h.pop_value();
        if (v == 1) ++ones;
        else if (v == 3) ++threes;
        else if (v == 5) ++fives;
    }
    EXPECT_EQ(ones, 2);
    EXPECT_EQ(threes, 2);
    EXPECT_EQ(fives, 3);
}

// 9. Custom comparator: max-heap
TEST(MinHeapInt, MaxHeapWithGreater) {
    imdb::MinHeap<int, std::greater<int>> h;
    for (int x : {1, 5, 3, 8, 2}) h.push(x);
    EXPECT_EQ(h.top(), 8);
}

// 10. Custom comparator with strings (lexicographic)
TEST(MinHeapStr, LexicographicComparator) {
    imdb::MinHeap<std::string> h;
    h.push("delta");
    h.push("alpha");
    h.push("charlie");
    h.push("bravo");
    EXPECT_EQ(h.top(), "alpha");
    EXPECT_EQ(h.pop_value(), "alpha");
    EXPECT_EQ(h.pop_value(), "bravo");
    EXPECT_EQ(h.pop_value(), "charlie");
    EXPECT_EQ(h.pop_value(), "delta");
}

// 11. Initializer-list ctor
TEST(MinHeapInt, InitializerListCtor) {
    imdb::MinHeap<int> h{9, 4, 1, 7, 2};
    EXPECT_EQ(h.size(), 5u);
    EXPECT_EQ(h.top(), 1);
}

// 12. Range ctor from std::vector
TEST(MinHeapInt, RangeCtor) {
    std::vector<int> v = {9, 4, 1, 7, 2};
    imdb::MinHeap<int> h(v.begin(), v.end());
    EXPECT_EQ(h.size(), 5u);
    EXPECT_EQ(h.top(), 1);
}

// 13. Copy ctor preserves contents
TEST(MinHeapInt, CopyCtor) {
    imdb::MinHeap<int> a;
    a.push(3);
    a.push(1);
    a.push(2);
    imdb::MinHeap<int> b = a;  // copy
    EXPECT_EQ(a.size(), b.size());
    EXPECT_EQ(a.top(), b.top());
    b.push(0);
    EXPECT_NE(a.top(), b.top());
    EXPECT_EQ(a.top(), 1);
    EXPECT_EQ(b.top(), 0);
}

// 14. Move ctor transfers contents
TEST(MinHeapInt, MoveCtor) {
    imdb::MinHeap<int> a;
    for (int i = 0; i < 100; ++i) a.push(i);
    imdb::MinHeap<int> b = std::move(a);
    EXPECT_EQ(b.size(), 100u);
    EXPECT_TRUE(a.empty());
    EXPECT_EQ(b.top(), 0);
}

// 15. Move-assign transfers contents
TEST(MinHeapInt, MoveAssign) {
    imdb::MinHeap<int> a;
    for (int i = 0; i < 50; ++i) a.push(50 - i);
    imdb::MinHeap<int> b;
    b.push(999);
    b = std::move(a);
    EXPECT_EQ(b.size(), 50u);
    EXPECT_EQ(b.top(), 1);
    EXPECT_TRUE(a.empty());
}

// 16. Emplace constructs in place
TEST(MinHeapInt, Emplace) {
    imdb::MinHeap<std::pair<int, std::string>> h;
    h.emplace(2, "two");
    h.emplace(1, "one");
    h.emplace(3, "three");
    EXPECT_EQ(h.top().first, 1);
    EXPECT_EQ(h.top().second, "one");
}

// 17. Large randomized round-trip (catches sift bugs)
TEST(MinHeapInt, RandomizedRoundTrip) {
    constexpr int N = 5'000;
    std::mt19937 rng(42);
    std::vector<int> v(N);
    for (int i = 0; i < N; ++i) v[static_cast<std::size_t>(i)] = static_cast<int>(rng());
    imdb::MinHeap<int> h(v.begin(), v.end());
    std::vector<int> sorted = v;
    std::sort(sorted.begin(), sorted.end());
    for (int x : sorted) {
        ASSERT_FALSE(h.empty());
        EXPECT_EQ(h.pop_value(), x);
    }
    EXPECT_TRUE(h.empty());
}

// 18. top() on empty heap asserts (death test)
TEST(MinHeapInt, TopOnEmptyAsserts) {
    imdb::MinHeap<int> h;
    EXPECT_DEATH(static_cast<void>(h.top()), "");
}

// 19. pop() on empty heap asserts
TEST(MinHeapInt, PopOnEmptyAsserts) {
    imdb::MinHeap<int> h;
    EXPECT_DEATH(h.pop(), "");
}

// 20. Clear empties the heap
TEST(MinHeapInt, Clear) {
    imdb::MinHeap<int> h;
    for (int i = 0; i < 100; ++i) h.push(i);
    h.clear();
    EXPECT_TRUE(h.empty());
    EXPECT_EQ(h.size(), 0u);
    h.push(7);
    EXPECT_EQ(h.top(), 7);
}

// 21. Swap exchanges contents
TEST(MinHeapInt, Swap) {
    imdb::MinHeap<int> a;
    a.push(1);
    a.push(2);
    imdb::MinHeap<int> b;
    b.push(100);
    a.swap(b);
    EXPECT_EQ(a.size(), 1u);
    EXPECT_EQ(a.top(), 100);
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.top(), 1);
}

// 22. Stress: Top-K pattern (insert N=10k, keep only K=10 best)
TEST(MinHeapInt, TopKPattern) {
    // We want the 10 largest elements.
    std::mt19937 rng(7);
    std::vector<int> data;
    for (int i = 0; i < 10'000; ++i) data.push_back(static_cast<int>(rng() % 100'000));
    std::vector<int> sorted = data;
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    std::vector<int> expected_top10(sorted.begin(), sorted.begin() + 10);

    // Min-heap of size K storing the K largest candidates seen so far.
    // The top of the min-heap is the smallest of those K, i.e. the
    // worst-of-best. We evict only when a newcomer beats it.
    imdb::MinHeap<int> topk;
    topk.reserve(10);
    for (int x : data) {
        if (topk.size() < 10) {
            topk.push(x);
        } else if (x > topk.top()) {  // x beats the current worst-of-best
            topk.pop();
            topk.push(x);
        }
    }
    std::vector<int> got;
    while (!topk.empty()) got.push_back(topk.pop_value());
    std::sort(got.begin(), got.end(), std::greater<int>());
    EXPECT_EQ(got, expected_top10);
}

}  // namespace
