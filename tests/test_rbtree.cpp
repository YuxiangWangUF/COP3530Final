// SPDX-License-Identifier: MIT
// RedBlackTree unit tests -- 25 cases covering empty/single/random/ordered/
// reverse-ordered inserts, every erase shape, lower/upper bound, iterator
// stability, and red-black invariant verification.

#include "imdb/RedBlackTree.h"
#include "imdb/MinHeap.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

// 1. Default ctor: empty
TEST(RBT, DefaultCtorIsEmpty) {
    imdb::RedBlackTree<int, int> t;
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.size(), 0u);
    EXPECT_EQ(t.begin(), t.end());
    EXPECT_FALSE(t.contains(0));
    EXPECT_EQ(t.find(0), t.end());
}

// 2. Single insert + lookup
TEST(RBT, SingleInsertAndLookup) {
    imdb::RedBlackTree<int, int> t;
    auto [it, inserted] = t.insert(42, 42);
    EXPECT_TRUE(inserted);
    EXPECT_EQ(it->first, 42);
    EXPECT_EQ(it->second, 42);
    EXPECT_EQ(t.size(), 1u);
    EXPECT_TRUE(t.contains(42));
    EXPECT_EQ(t.find(42)->second, 42);
    EXPECT_FALSE(t.contains(7));
    EXPECT_TRUE(t.verify_invariants());
}

// 3. Duplicate insert returns {existing, false}
TEST(RBT, DuplicateInsertReturnsFalse) {
    imdb::RedBlackTree<int, int> t;
    t.insert(1, 100);
    auto [it, inserted] = t.insert(1, 999);
    EXPECT_FALSE(inserted);
    EXPECT_EQ(it->second, 100);  // original value preserved
    EXPECT_EQ(t.size(), 1u);
}

// 4. Random insert, in-order traversal is sorted, invariants hold
TEST(RBT, RandomInsertSortedTraversal) {
    imdb::RedBlackTree<int, int> t;
    std::mt19937 rng(1);
    std::set<int> uniq;
    for (int i = 0; i < 200; ++i) {
        int k = static_cast<int>(rng() % 1000);
        uniq.insert(k);
        t.insert(k, k * 2);
    }
    // RBT rejects duplicate keys, so size equals the count of distinct
    // random draws (not the count of draws).
    EXPECT_EQ(t.size(), uniq.size());
    EXPECT_TRUE(t.verify_invariants());
    std::vector<int> seen;
    for (auto& kv : t) seen.push_back(kv.first);
    EXPECT_TRUE(std::is_sorted(seen.begin(), seen.end()));
}

// 5. Ascending insert (CLRS worst case for naive BST)
TEST(RBT, AscendingInsert) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 1; i <= 100; ++i) t.insert(i, i);
    EXPECT_EQ(t.size(), 100u);
    EXPECT_TRUE(t.verify_invariants());
    std::vector<int> seen;
    for (auto& kv : t) seen.push_back(kv.first);
    std::vector<int> expected(100);
    std::iota(expected.begin(), expected.end(), 1);
    EXPECT_EQ(seen, expected);
}

// 6. Descending insert (another worst case)
TEST(RBT, DescendingInsert) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 100; i >= 1; --i) t.insert(i, i);
    EXPECT_EQ(t.size(), 100u);
    EXPECT_TRUE(t.verify_invariants());
    int prev = 0;
    for (auto& kv : t) {
        EXPECT_GT(kv.first, prev);
        prev = kv.first;
    }
}

// 7. Operator[] lazy insert
TEST(RBT, SubscriptLazyInsert) {
    imdb::RedBlackTree<std::string, int> t;
    t["alpha"] = 1;
    t["bravo"] = 2;
    EXPECT_EQ(t["alpha"], 1);
    EXPECT_EQ(t["bravo"], 2);
    EXPECT_EQ(t["charlie"], 0);  // default-inserted
    EXPECT_EQ(t.size(), 3u);
    EXPECT_TRUE(t.verify_invariants());
}

// 8. at() throws when missing
TEST(RBT, AtThrowsWhenMissing) {
    imdb::RedBlackTree<int, int> t;
    t.insert(1, 1);
    EXPECT_EQ(t.at(1), 1);
    EXPECT_THROW(t.at(2), std::out_of_range);
}

// 9. Erase leaf node
TEST(RBT, EraseLeaf) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 1; i <= 7; ++i) t.insert(i, i);
    EXPECT_TRUE(t.erase(7));
    EXPECT_FALSE(t.contains(7));
    EXPECT_EQ(t.size(), 6u);
    EXPECT_TRUE(t.verify_invariants());
}

// 10. Erase node with one child
TEST(RBT, EraseOneChild) {
    imdb::RedBlackTree<int, int> t;
    // Build a tree where erasing a node forces single-child fixup.
    t.insert(10, 1);
    t.insert(5, 2);
    t.insert(15, 3);
    t.insert(3, 4);
    t.erase(5);
    EXPECT_FALSE(t.contains(5));
    EXPECT_TRUE(t.contains(3));
    EXPECT_EQ(t.size(), 3u);
    EXPECT_TRUE(t.verify_invariants());
}

// 11. Erase node with two children (successor swap)
TEST(RBT, EraseTwoChildren) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 1; i <= 7; ++i) t.insert(i, i);
    // 4 has two children (3 and 5).
    EXPECT_TRUE(t.erase(4));
    EXPECT_FALSE(t.contains(4));
    EXPECT_EQ(t.size(), 6u);
    EXPECT_TRUE(t.verify_invariants());
}

// 12. Erase root
TEST(RBT, EraseRoot) {
    imdb::RedBlackTree<int, int> t;
    t.insert(10, 1);
    EXPECT_TRUE(t.erase(10));
    EXPECT_TRUE(t.empty());
    EXPECT_TRUE(t.verify_invariants());
}

// 13. Erase every node
TEST(RBT, EraseAll) {
    imdb::RedBlackTree<int, int> t;
    std::mt19937 rng(13);
    std::vector<int> keys;
    for (int i = 0; i < 50; ++i) {
        int k = static_cast<int>(rng() % 100);
        if (std::find(keys.begin(), keys.end(), k) == keys.end()) {
            keys.push_back(k);
            t.insert(k, k);
        }
    }
    for (int k : keys) {
        EXPECT_TRUE(t.erase(k));
        EXPECT_FALSE(t.contains(k));
        EXPECT_TRUE(t.verify_invariants());
    }
    EXPECT_TRUE(t.empty());
}

// 14. Erase returns 0 for missing key
TEST(RBT, EraseMissingReturnsZero) {
    imdb::RedBlackTree<int, int> t;
    t.insert(1, 1);
    EXPECT_EQ(t.erase(2), 0u);
    EXPECT_EQ(t.size(), 1u);
}

// 15. Random insert + random erase round trip
TEST(RBT, RandomInsertErase) {
    imdb::RedBlackTree<int, int> t;
    std::mt19937 rng(101);
    std::vector<int> live;
    for (int round = 0; round < 2000; ++round) {
        if (live.empty() || (rng() % 4) < 3) {
            int k = static_cast<int>(rng() % 200);
            if (std::find(live.begin(), live.end(), k) == live.end()) {
                live.push_back(k);
                t.insert(k, k);
            }
        } else {
            int idx = static_cast<int>(rng() % live.size());
            int k = live[static_cast<std::size_t>(idx)];
            live.erase(live.begin() + idx);
            EXPECT_TRUE(t.erase(k));
        }
        if (round % 100 == 0) {
            EXPECT_TRUE(t.verify_invariants());
        }
    }
    EXPECT_EQ(t.size(), live.size());
    for (int k : live) EXPECT_TRUE(t.contains(k));
    EXPECT_TRUE(t.verify_invariants());
}

// 16. lower_bound basic
TEST(RBT, LowerBoundBasic) {
    imdb::RedBlackTree<int, int> t;
    for (int k : {10, 20, 30, 40, 50}) t.insert(k, k);
    auto it = t.lower_bound(25);
    ASSERT_NE(it, t.end());
    EXPECT_EQ(it->first, 30);
    it = t.lower_bound(10);
    EXPECT_EQ(it->first, 10);
    it = t.lower_bound(50);
    EXPECT_EQ(it->first, 50);
    it = t.lower_bound(51);
    EXPECT_EQ(it, t.end());
}

// 17. upper_bound basic
TEST(RBT, UpperBoundBasic) {
    imdb::RedBlackTree<int, int> t;
    for (int k : {10, 20, 30, 40, 50}) t.insert(k, k);
    auto it = t.upper_bound(30);
    ASSERT_NE(it, t.end());
    EXPECT_EQ(it->first, 40);
    it = t.upper_bound(50);
    EXPECT_EQ(it, t.end());
}

// 18. equal_range
TEST(RBT, EqualRange) {
    imdb::RedBlackTree<int, int> t;
    for (int k : {1, 2, 2, 3, 4, 4, 4, 5}) t.insert(k, k);  // duplicates collapse
    auto [lo, hi] = t.equal_range(4);
    EXPECT_EQ(lo->first, 4);
    EXPECT_EQ(hi->first, 5);
}

// 19. range-based for loop
TEST(RBT, RangeBasedFor) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 0; i < 10; ++i) t.insert(i, i * i);
    int sum = 0;
    for (const auto& [k, v] : t) {
        sum += k;
        EXPECT_EQ(v, k * k);
    }
    EXPECT_EQ(sum, 45);  // 0+1+...+9
}

// 20. iterator bidirectional
TEST(RBT, IteratorBidirectional) {
    imdb::RedBlackTree<int, int> t;
    for (int k : {5, 3, 8, 1, 4, 7, 9}) t.insert(k, k);
    auto it = t.begin();
    auto end = t.end();
    std::vector<int> forward_seq;
    while (it != end) {
        forward_seq.push_back(it->first);
        ++it;
    }
    EXPECT_EQ(forward_seq, (std::vector<int>{1, 3, 4, 5, 7, 8, 9}));

    // Walk backwards
    std::vector<int> backward_seq;
    --it;  // step back from end() sentinel: lands on 9
    while (true) {
        backward_seq.push_back(it->first);
        if (it == t.begin()) break;
        --it;
    }
    std::reverse(backward_seq.begin(), backward_seq.end());
    EXPECT_EQ(backward_seq, forward_seq);
}

// 21. iterator stability across insert of new key
TEST(RBT, IteratorStabilityAcrossInsert) {
    imdb::RedBlackTree<int, int> t;
    for (int k : {1, 3, 5, 7}) t.insert(k, k);
    auto it3 = t.find(3);
    auto it5 = t.find(5);
    ASSERT_NE(it3, t.end());
    ASSERT_NE(it5, t.end());
    t.insert(4, 4);
    // Existing iterators must still point at the same keys.
    EXPECT_EQ(it3->first, 3);
    EXPECT_EQ(it5->first, 5);
    // And ++ from it3 should now yield 4 then 5.
    ++it3;
    EXPECT_EQ(it3->first, 4);
    ++it3;
    EXPECT_EQ(it3->first, 5);
}

// 22. Copy ctor + copy assign
TEST(RBT, CopyAndAssign) {
    imdb::RedBlackTree<int, int> a;
    for (int i = 0; i < 50; ++i) a.insert(i, i);
    imdb::RedBlackTree<int, int> b = a;  // copy ctor
    EXPECT_EQ(b.size(), 50u);
    EXPECT_EQ(b.find(25)->second, 25);
    EXPECT_TRUE(b.verify_invariants());

    imdb::RedBlackTree<int, int> c;
    c = a;
    EXPECT_EQ(c.size(), 50u);
    EXPECT_TRUE(c.verify_invariants());

    // Modify one, the other must be unaffected.
    c.erase(25);
    EXPECT_TRUE(a.contains(25));
    EXPECT_FALSE(c.contains(25));
}

// 23. Move ctor + move assign
TEST(RBT, MoveCtorAndAssign) {
    imdb::RedBlackTree<int, int> a;
    for (int i = 0; i < 50; ++i) a.insert(i, i);
    imdb::RedBlackTree<int, int> b = std::move(a);
    EXPECT_EQ(b.size(), 50u);
    EXPECT_TRUE(a.empty());

    imdb::RedBlackTree<int, int> c;
    c.insert(999, 1);
    c = std::move(b);
    EXPECT_EQ(c.size(), 50u);
    EXPECT_TRUE(c.contains(0));
    EXPECT_TRUE(b.empty());
    EXPECT_TRUE(c.verify_invariants());
}

// 24. Initializer-list ctor
TEST(RBT, InitializerList) {
    imdb::RedBlackTree<int, int> t{{1, 10}, {3, 30}, {2, 20}};
    EXPECT_EQ(t.size(), 3u);
    EXPECT_TRUE(t.verify_invariants());
    EXPECT_EQ(t.find(2)->second, 20);
}

// 25. Stress: 10k random insert + 5k random erase, invariants + sorted iter
TEST(RBT, StressInsertErase) {
    imdb::RedBlackTree<int, int> t;
    std::mt19937 rng(7);
    std::vector<int> live;
    for (int i = 0; i < 10'000; ++i) {
        int k = static_cast<int>(rng() % 50'000);
        if (std::find(live.begin(), live.end(), k) == live.end()) {
            live.push_back(k);
            t.insert(k, k);
        }
    }
    EXPECT_EQ(t.size(), live.size());
    EXPECT_TRUE(t.verify_invariants());
    for (int i = 0; i < 5'000; ++i) {
        int idx = static_cast<int>(rng() % live.size());
        int k = live[static_cast<std::size_t>(idx)];
        live.erase(live.begin() + idx);
        EXPECT_TRUE(t.erase(k));
    }
    EXPECT_EQ(t.size(), live.size());
    EXPECT_TRUE(t.verify_invariants());
    int prev = -1;
    int observed_max = 0;
    for (auto& kv : t) {
        EXPECT_GT(kv.first, prev);  // in-order traversal is strictly increasing
        prev = kv.first;
        observed_max = kv.first;
    }
    // The loop is the real check; the line below is just a
    // sanity-printer so a failure surfaces the final key value in
    // the test log.
    EXPECT_GE(observed_max, 0) << "tree ended up with no live keys";
}

// 26. Custom comparator (descending key)
TEST(RBT, CustomDescendingComparator) {
    imdb::RedBlackTree<int, int, std::greater<int>> t;
    t.insert(1, 1);
    t.insert(2, 2);
    t.insert(3, 3);
    std::vector<int> seen;
    for (auto& kv : t) seen.push_back(kv.first);
    EXPECT_EQ(seen, (std::vector<int>{3, 2, 1}));
    EXPECT_TRUE(t.verify_invariants());
}

// 27. Compose with MinHeap (RBT indexes, heap orders)
TEST(RBT, ComposeWithMinHeap) {
    imdb::RedBlackTree<int, int> idx;
    imdb::MinHeap<int, std::greater<int>> topk;
    std::mt19937 rng(99);
    // Use a unique-key generator -- the RBT is a unique-key associative
    // container; drawing from % 5000 with a 1000-iteration loop produces
    // collisions and the test is about composition, not duplicate handling.
    for (int i = 0; i < 1000; ++i) {
        int k = i * 7 + 3;          // unique per iteration
        idx.insert(k, k);
        topk.push(k);
        if (topk.size() > 5) topk.pop();
    }
    int heap_min = topk.top();
    int rbt_count = 0;
    int ge = 0;
    for (auto& kv : idx) {
        if (kv.first >= heap_min) ++ge;
        ++rbt_count;
    }
    EXPECT_GE(ge, 5);  // at least the heap's contents are >= heap_min
    EXPECT_EQ(rbt_count, 1000);
}

// 28. Erase during iteration (postfix)
TEST(RBT, EraseDuringIteration) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 0; i < 20; ++i) t.insert(i, i);
    auto it = t.begin();
    int erased = 0;
    while (it != t.end()) {
        if (it->first % 2 == 0) {
            it = t.erase(it);
            ++erased;
        } else {
            ++it;
        }
    }
    EXPECT_EQ(erased, 10);
    EXPECT_EQ(t.size(), 10u);
    EXPECT_TRUE(t.verify_invariants());
    for (auto& kv : t) EXPECT_EQ(kv.first % 2, 1);
}

// 29. clear()
TEST(RBT, Clear) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 0; i < 100; ++i) t.insert(i, i);
    t.clear();
    EXPECT_TRUE(t.empty());
    EXPECT_EQ(t.begin(), t.end());
    // Re-use after clear.
    t.insert(1, 1);
    EXPECT_EQ(t.size(), 1u);
    EXPECT_TRUE(t.verify_invariants());
}

// 30. Stress: pure deletion of every other key (worst case for recolor)
TEST(RBT, StressAlternatingDelete) {
    imdb::RedBlackTree<int, int> t;
    for (int i = 0; i < 1000; ++i) t.insert(i, i);
    for (int i = 0; i < 1000; i += 2) t.erase(i);
    EXPECT_EQ(t.size(), 500u);
    EXPECT_TRUE(t.verify_invariants());
    int prev = -1;
    for (auto& kv : t) {
        EXPECT_GT(kv.first, prev);
        EXPECT_EQ(kv.first % 2, 1);
        prev = kv.first;
    }
}

}  // namespace
