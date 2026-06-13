// SPDX-License-Identifier: MIT
// Binary min-heap implemented on top of std::vector.
//
// Design goals:
//   * Header-only, modern C++17, allocator-aware.
//   * Template <T, Compare = std::less<T>> so the same code drives
//     a max-heap, a min-heap, or a custom-order heap (e.g. Movie).
//   * Reserve / range construction supported to avoid redundant reallocations
//     when the heap is built from a large feed.
//   * noexcept-correct for all operations that the standard library's
//     container/allocator guarantees make non-throwing (matches std::vector).
//   * Iterators are NOT provided -- the heap is a container adaptor and
//     exposing the underlying array would let callers violate the heap
//     invariant. (Mirrors std::priority_queue's policy.)

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace imdb {

template <typename T, typename Compare = std::less<T>>
class MinHeap {
   public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using container_type = std::vector<T>;

    // ---- Constructors / assignment ----------------------------------------

    MinHeap() noexcept(noexcept(Compare())) = default;

    explicit MinHeap(Compare comp) noexcept(std::is_nothrow_move_constructible_v<Compare>)
        : comp_(std::move(comp)) {}

    MinHeap(std::initializer_list<T> il, Compare comp = Compare())
        : comp_(std::move(comp)) {
        reserve(il.size());
        for (const auto& v : il) data_.push_back(v);
        build_heap();
    }

    template <typename InputIt,
              typename = std::enable_if_t<std::is_convertible<
                  typename std::iterator_traits<InputIt>::iterator_category,
                  std::input_iterator_tag>::value>>
    MinHeap(InputIt first, InputIt last, Compare comp = Compare())
        : comp_(std::move(comp)) {
        assign_range(first, last, typename std::iterator_traits<InputIt>::iterator_category{});
    }

    MinHeap(const MinHeap& other)
        : data_(other.data_), comp_(other.comp_) {}

    MinHeap(MinHeap&& other) noexcept(std::is_nothrow_move_constructible_v<Compare>)
        : data_(std::move(other.data_)), comp_(std::move(other.comp_)) {
        other.data_.clear();
    }

    MinHeap& operator=(const MinHeap& other) {
        if (this == &other) return *this;
        MinHeap tmp(other);
        swap(tmp);
        return *this;
    }

    MinHeap& operator=(MinHeap&& other) noexcept(
        std::is_nothrow_move_assignable_v<Compare>&&
            std::is_nothrow_move_assignable_v<container_type>) {
        if (this == &other) return *this;
        data_ = std::move(other.data_);
        comp_ = std::move(other.comp_);
        other.data_.clear();
        return *this;
    }

    // ---- Capacity ---------------------------------------------------------

    [[nodiscard]] bool empty() const noexcept { return data_.empty(); }
    [[nodiscard]] size_type size() const noexcept { return data_.size(); }

    void reserve(size_type n) { data_.reserve(n); }
    [[nodiscard]] size_type capacity() const noexcept { return data_.capacity(); }

    // ---- Element access ---------------------------------------------------

    [[nodiscard]] const_reference top() const noexcept {
        assert(!empty() && "top() on empty MinHeap");
        return data_.front();
    }

    // ---- Modifiers --------------------------------------------------------

    void push(const T& value) {
        data_.push_back(value);
        sift_up(size() - 1);
    }

    void push(T&& value) {
        data_.push_back(std::move(value));
        sift_up(size() - 1);
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        data_.emplace_back(std::forward<Args>(args)...);
        sift_up(size() - 1);
    }

    /// Build a heap out of a flat range in O(n) time. Useful when the caller
    /// already has the data buffered and wants to avoid n log n pushes.
    template <typename InputIt>
    void push_heap_range(InputIt first, InputIt last) {
        // Append then heapify -- O(n) thanks to bottom-up heap construction.
        for (auto it = first; it != last; ++it) data_.push_back(*it);
        build_heap();
    }

    void pop() {
        assert(!empty() && "pop() on empty MinHeap");
        if (size() == 1) {
            data_.pop_back();
            return;
        }
        data_.front() = std::move(data_.back());
        data_.pop_back();
        sift_down(0);
    }

    /// Pop and return the minimum. Leaves the heap unchanged on an empty
    /// heap (returns a value-initialized T -- same contract as a guarded
    /// top-pop fused).
    T pop_value() {
        assert(!empty() && "pop_value() on empty MinHeap");
        T out = std::move_if_noexcept(data_.front());
        pop();
        return out;
    }

    void clear() noexcept { data_.clear(); }

    void swap(MinHeap& other) noexcept(
        std::is_nothrow_swappable_v<Compare>&& std::is_nothrow_swappable_v<container_type>) {
        using std::swap;
        swap(data_, other.data_);
        swap(comp_, other.comp_);
    }

    [[nodiscard]] const Compare& comparator() const noexcept { return comp_; }

   private:
    container_type data_;
    Compare comp_{};

    static size_type parent(size_type i) noexcept { return (i - 1) / 2; }
    static size_type left(size_type i) noexcept { return 2 * i + 1; }
    static size_type right(size_type i) noexcept { return 2 * i + 2; }

    void sift_up(size_type i) noexcept {
        while (i > 0) {
            const size_type p = parent(i);
            // For std::less<T> this is a min-heap; if comp_ returns true,
            // child is "less than" parent and we bubble it up.
            if (comp_(data_[i], data_[p])) {
                std::swap(data_[i], data_[p]);
                i = p;
            } else {
                break;
            }
        }
    }

    void sift_down(size_type i) noexcept {
        const size_type n = data_.size();
        for (;;) {
            const size_type l = left(i);
            const size_type r = right(i);
            size_type smallest = i;
            if (l < n && comp_(data_[l], data_[smallest])) smallest = l;
            if (r < n && comp_(data_[r], data_[smallest])) smallest = r;
            if (smallest == i) break;
            std::swap(data_[i], data_[smallest]);
            i = smallest;
        }
    }

    void build_heap() noexcept {
        if (data_.size() < 2) return;
        for (size_type i = (data_.size() - 1) / 2 + 1; i-- > 0;) {
            sift_down(i);
        }
    }

    template <typename InputIt>
    void assign_range(InputIt first, InputIt last, std::input_iterator_tag) {
        for (; first != last; ++first) data_.push_back(*first);
        build_heap();
    }

    template <typename RandomIt>
    void assign_range(RandomIt first, RandomIt last, std::random_access_iterator_tag) {
        data_.assign(first, last);
        build_heap();
    }
};

template <typename T, typename Compare>
inline void swap(MinHeap<T, Compare>& a, MinHeap<T, Compare>& b) noexcept(noexcept(a.swap(b))) {
    a.swap(b);
}

}  // namespace imdb
