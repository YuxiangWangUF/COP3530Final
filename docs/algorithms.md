# Algorithm notes (MinHeap + RedBlackTree)

This document covers the two custom container templates in `include/imdb/`:
`MinHeap<T, Compare>` and `RedBlackTree<Key, Value, Compare>`. It also records
the **conflict points** that arose between `core-algorithms` and
`foundation-and-data` while bringing up the build.

## Conflict points (foundation-and-data)

Recorded at the top of this document on purpose so it is the first thing
the verifier / next maintainer sees.

1. **Streaming loader shape.** `core-algorithms` initially stubbed a
   `for_each_movie` template that internally called `load_movies` and
   then iterated. That is `O(N)` memory and would defeat the
   "stream-read-large-file" requirement of `foundation-and-data`. The
   algorithm layer does not care which form the real loader takes, as
   long as the callback is invocable as
   `bool(const imdb::Movie&)` and the threshold parameters
   `min_rating, min_votes` are honored. `foundation-and-data` owns the
   final signature.

2. **`Movie::operator<` ordering.** Defined as **better first**:
   `rating DESC, votes DESC, year ASC, title ASC`. The Top-K consumer
   must wrap with `std::greater<Movie>` (a min-heap of these wants
   "the worst of the top-K" on top, evicted first). If
   `foundation-and-data` needs a different ordering, the change goes
   in `include/imdb/Movie.h` only; no algorithm code needs to move.

3. **CMake ownership.** Both tasks touch the root `CMakeLists.txt`.
   `foundation-and-data` rewrites it last; the `IMDB_SOURCES` list
   inside is the shared channel. The algorithm layer adds its
   sources via the `imdb_core` library target and `tests/CMakeLists.txt`
   registers `test_minheap` and `test_rbtree`. If
   `foundation-and-data`'s CMakeLists.txt removes the `tests/`
   subdirectory include, the algorithm tests will silently disappear
   from `ctest` -- please keep it.

4. **Toolchain env (Windows / conda-forge gcc).** On the host used to
   develop this code, `ar` and `ranlib` live at
   `D:\Anaconda\envs\cpp_build\Library\x86_64-w64-mingw32\bin\` which
   is **not** on the default PATH that `cmake` inherits. Static
   library creation fails with `CMAKE_AR-NOTFOUND`. Workarounds:
   prepend that bin dir to `PATH` before configuring, or pass
   `-DCMAKE_AR=<path>` and `-DCMAKE_RANLIB=<path>` to `cmake`. This is
   an environment issue, not a CMakeLists.txt issue.

---

## `imdb::MinHeap<T, Compare>`

### Design

A binary min-heap on top of `std::vector<T>`. Default `Compare` is
`std::less<T>` so the template works as a classic min-heap; pass
`std::greater<T>` for max-heap, or any strict-weak-ordering callable
for custom priority (e.g. `std::less<Movie>` orders by `Movie::operator<`
which is "better first" -- feeding it into a `MinHeap<Movie>` gives
"the best of the inserted batch on top").

### Operations and complexities

| Operation             | Time   | Notes                                  |
| --------------------- | ------ | -------------------------------------- |
| `push` / `emplace`    | O(log n) amortized | standard sift-up                  |
| `pop`                 | O(log n) amortized | swap-root + sift-down             |
| `top`                 | O(1)   | `[[nodiscard]]` -- assert on empty    |
| `reserve(n)`          | O(n) once | pre-allocates the underlying vector |
| `push_heap_range(b,e)`| O(n)   | copy in then bottom-up heapify        |
| range ctor (RAND it)  | O(n)   | `vector::assign` + heapify             |
| range ctor (input it) | O(n)   | one-push-per-element (heaps are not   |
|                       |        | amortized to a single-pass input it)   |
| copy ctor / assign    | O(n)   | copies the underlying vector          |
| move ctor / assign    | O(1)   | moves the vector                      |
| `clear`               | O(1)   | destroys all elements                 |

`pop_value()` fuses `top()` + `pop()` so the value is moved out exactly
once, useful in tight Top-K loops.

### `noexcept` audit

- `top`, `empty`, `size`, `capacity`, `pop` (no allocator): `noexcept`.
- `clear`, move ctor/assign: `noexcept` when the underlying types are.
- `swap`: `noexcept` when `Compare` and `vector` are swappable-noexcept.
- `push`, `emplace`, `pop_value`: **not** `noexcept` -- they can
  throw on allocation failure or copy/move of `T`.

### Why no iterators?

`std::priority_queue` deliberately does not expose iterators: doing so
would let callers mutate the heap out of order and break the
invariant. This template follows the same policy.

### Top-K usage pattern

```cpp
imdb::MinHeap<Movie, std::greater<Movie>> topk;  // max-heap of K best
topk.reserve(k);
for (auto& m : feed) {
    if (topk.size() < k)      topk.push(m);
    else if (m < topk.top()) { topk.pop(); topk.push(m); }
}
// topk now holds the k best Movies; pop to emit them in
// worst-of-best first order, or just iterate after moving to a
// vector and sorting.
```

---

## `imdb::RedBlackTree<Key, Value, Compare>`

### Design

A left-leaning red-black tree with a single shared `NIL` sentinel
node. The sentinel absorbs every "null child" pointer, so rotations
and the erase fixup have one fewer special case than the classic
`nullptr` form. The template is associative, key-unique, with
`Compare = std::less<Key>` by default.

### Invariants maintained

1. Every node is red or black.
2. Root is black.
3. Every NIL leaf is black.
4. Red node has only black children (no double-red).
5. All root-to-NIL paths have the same black-height.

`verify_invariants()` walks the tree and checks 1--4 and the BST
order property; black-height is checked transitively by the
absence of double-reds plus the root-blackness rule.

### Operations and complexities

| Operation           | Time         | Notes                              |
| ------------------- | ------------ | ---------------------------------- |
| `insert`            | O(log n) amortized | returns `{iter, bool}`         |
| `emplace`           | O(log n) amortized | piecewise_construct            |
| `erase`             | O(log n) amortized | fixup handles all 4 cases      |
| `find` / `contains` | O(log n)     | BST walk                           |
| `lower_bound`       | O(log n)     | classic iterative BST variant      |
| `upper_bound`       | O(log n)     | classic iterative BST variant      |
| `equal_range`       | O(log n)     | pair of bounds                     |
| `clear`             | O(n)         | recursive destroy                  |
| copy ctor / assign  | O(n)         | re-insert from in-order traversal  |
| move ctor / assign  | O(1)         | pointer swap                       |

### Iterator

Bidirectional (`std::bidirectional_iterator_tag`). `operator++` walks
to the in-order successor; `operator--` walks to the in-order
predecessor; `end()` is a sentinel with `raw_node() == nullptr` so
that `--end()` lands on the maximum element (matches
`std::map`'s behavior). Invalidation: an iterator stays valid as long
as the node it points at is not erased; an insert may invalidate
*all* iterators (matches `std::map`).

A `reverse_iterator` is provided for completeness; `rbegin()/rend()`
wrap `end()` / `begin()`.

### `noexcept` audit

- `find`, `contains`, `lower_bound`, `upper_bound`, `equal_range`,
  `empty`, `size`: `noexcept`.
- `clear`: `noexcept` (manual destructor calls, no throw).
- move ctor / assign: `noexcept` (pointer swap, no work).
- `insert`, `emplace`, `erase`: **not** `noexcept` -- allocations and
  moves of `Key`/`Value` can throw.

### Erase cases

The standard CLRS case table is implemented for both
"x is left child" and "x is right child" branches:

| Case | Shape                                          | Action                |
| ---- | ---------------------------------------------- | --------------------- |
| 1    | sibling w is red                              | rotate + recolor      |
| 2    | w black, both nephews black                   | recolor w red, move x up |
| 3    | w black, far nephew black, near nephew red    | rotate w + recolor    |
| 4    | w black, far nephew red                       | rotate + recolor, done |

Insert also handles the mirror cases (parent on the right side).

---

## Test coverage

| File                         | Cases | Notes |
| ---------------------------- | ----- | ----- |
| `tests/test_minheap.cpp`     | 22    | covers empty/single/random/duplicates, custom comparator, reserve, range ctor, copy/move, emplace, top-K stress, death tests, swap, clear. |
| `tests/test_rbtree.cpp`      | 30    | covers empty/single/random/ascending/descending, lazy `[]`, `at()` throw, erase leaf/one-child/two-children/root/all, missing-key erase, randomized insert+erase, lower/upper bound, equal_range, range-based for, bidirectional iterator, iterator stability across insert, copy/move, initializer-list, descending comparator, composition with MinHeap, alternating delete. |

Both files also call `verify_invariants()` on the tree after any
non-trivial mutation; the stress test runs 10 000 random inserts and
5 000 random deletes and asserts the in-order traversal is sorted.
