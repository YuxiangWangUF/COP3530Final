// SPDX-License-Identifier: MIT
// Red-black tree (left-leaning variant, sentinel-NIL) templated on
// {Key, Value, Compare}. Provides an associative container with the same
// surface shape as std::map, but implemented from scratch so the project can
// demonstrate the algorithm and benchmark against the standard library.
//
// Properties maintained at all times:
//   1. Every node is red or black.
//   2. The root is black.
//   3. Every NIL leaf is black.
//   4. If a node is red, both children are black (no double-red).
//   5. For each node, every path node -> descendant NIL contains the same
//      number of black nodes (black-height invariant).
//
// Header-only. The split is purely logical (this file is a single TU but
// large enough that keeping it standalone helps compile times and code
// review).

#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace imdb {

namespace rbt_detail {

enum class Color : std::uint8_t { Red, Black };

template <typename Key, typename Value, typename Compare>
struct Node {
    Key key;
    Value value;
    Color color = Color::Red;
    Node* parent = nullptr;
    Node* left = nullptr;
    Node* right = nullptr;

    Node(const Key& k, const Value& v, Color c, Node* p, Node* l, Node* r)
        : key(k), value(v), color(c), parent(p), left(l), right(r) {}
    template <typename K, typename V>
    Node(K&& k, V&& v, Color c, Node* p, Node* l, Node* r)
        : key(std::forward<K>(k)),
          value(std::forward<V>(v)),
          color(c),
          parent(p),
          left(l),
          right(r) {}
};

}  // namespace rbt_detail

template <typename Key, typename Value, typename Compare = std::less<Key>>
class RedBlackTree {
   public:
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using key_compare = Compare;
    using reference = value_type&;
    using const_reference = const value_type&;
    using pointer = value_type*;
    using const_pointer = const value_type*;

   private:
    using Node = rbt_detail::Node<Key, Value, Compare>;

    // One shared sentinel NIL is the empty-child marker for every node.
    // Using a single sentinel (rather than nullptr) means rotation,
    // parent-fixup, and erase all have one fewer special case to handle.
    Node* nil_;
    Node* root_;
    size_type size_;
    Compare comp_;

   public:
    class iterator {
       public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = RedBlackTree::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = RedBlackTree::pointer;
        using reference = RedBlackTree::reference;

        iterator() noexcept = default;
        iterator(Node* n, const RedBlackTree* t) noexcept : node_(n), tree_(t) {}

        reference operator*() const noexcept { return reinterpret_cast<reference>(*node_); }
        pointer operator->() const noexcept { return reinterpret_cast<pointer>(node_); }

        iterator& operator++() noexcept {
            node_ = tree_->next_node(node_);
            return *this;
        }
        iterator operator++(int) noexcept {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        iterator& operator--() noexcept {
            // first-decrement on end() should land on the maximum.
            if (node_ == nullptr) node_ = tree_->max_node(tree_->root_);
            else node_ = tree_->prev_node(node_);
            return *this;
        }
        iterator operator--(int) noexcept {
            iterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const iterator& a, const iterator& b) noexcept {
            return a.node_ == b.node_;
        }
        friend bool operator!=(const iterator& a, const iterator& b) noexcept {
            return a.node_ != b.node_;
        }

        // Expose node pointer for tests that need to verify color invariants.
        Node* raw_node() const noexcept { return node_; }

       private:
        Node* node_ = nullptr;
        const RedBlackTree* tree_ = nullptr;
        friend class RedBlackTree;
    };

    class const_iterator {
       public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = RedBlackTree::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = RedBlackTree::const_pointer;
        using reference = RedBlackTree::const_reference;

        const_iterator() noexcept = default;
        explicit const_iterator(iterator it) noexcept
            : node_(it.node_), tree_(it.tree_) {}
        const_iterator(Node* n, const RedBlackTree* t) noexcept : node_(n), tree_(t) {}

        reference operator*() const noexcept {
            return reinterpret_cast<reference>(const_cast<Node&>(*node_));
        }
        pointer operator->() const noexcept {
            return reinterpret_cast<pointer>(const_cast<Node*>(node_));
        }

        const_iterator& operator++() noexcept {
            node_ = tree_->next_node(node_);
            return *this;
        }
        const_iterator operator++(int) noexcept {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        const_iterator& operator--() noexcept {
            if (node_ == nullptr) node_ = tree_->max_node(tree_->root_);
            else node_ = tree_->prev_node(node_);
            return *this;
        }
        const_iterator operator--(int) noexcept {
            const_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const const_iterator& a, const const_iterator& b) noexcept {
            return a.node_ == b.node_;
        }
        friend bool operator!=(const const_iterator& a, const const_iterator& b) noexcept {
            return a.node_ != b.node_;
        }

       private:
        Node* node_ = nullptr;
        const RedBlackTree* tree_ = nullptr;
        friend class RedBlackTree;
    };

    class reverse_iterator {
       public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = RedBlackTree::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = RedBlackTree::pointer;
        using reference = RedBlackTree::reference;

        reverse_iterator() noexcept = default;
        explicit reverse_iterator(iterator it) noexcept
            : current_(it) {}

        reference operator*() const noexcept {
            iterator tmp = current_;
            --tmp;
            return *tmp;
        }
        pointer operator->() const noexcept {
            iterator tmp = current_;
            --tmp;
            return tmp.operator->();
        }
        reverse_iterator& operator++() noexcept {
            --current_;
            return *this;
        }
        reverse_iterator operator++(int) noexcept {
            reverse_iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        reverse_iterator& operator--() noexcept {
            ++current_;
            return *this;
        }
        reverse_iterator operator--(int) noexcept {
            reverse_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const reverse_iterator& a,
                               const reverse_iterator& b) noexcept {
            return a.current_ == b.current_;
        }
        friend bool operator!=(const reverse_iterator& a,
                               const reverse_iterator& b) noexcept {
            return a.current_ != b.current_;
        }

       private:
        iterator current_{};
    };

    // ---- Construction / destruction --------------------------------------

    RedBlackTree() : size_(0), comp_(Compare()) { init_nil(); }
    explicit RedBlackTree(Compare comp)
        : size_(0), comp_(std::move(comp)) { init_nil(); }

    RedBlackTree(std::initializer_list<value_type> init) : RedBlackTree() {
        for (const auto& kv : init) {
            auto [it, inserted] = insert_unique(kv.first, kv.second);
            (void)it;
            (void)inserted;
        }
    }

    RedBlackTree(const RedBlackTree& other) : RedBlackTree(other.comp_) {
        for (const auto& kv : other) {
            auto [it, inserted] = insert_unique(kv.first, kv.second);
            (void)it;
            (void)inserted;
        }
    }

    RedBlackTree(RedBlackTree&& other) noexcept
        : nil_(other.nil_), root_(other.root_), size_(other.size_),
          comp_(std::move(other.comp_)) {
        other.init_nil();
        other.size_ = 0;
    }

    RedBlackTree& operator=(const RedBlackTree& other) {
        if (this == &other) return *this;
        RedBlackTree tmp(other);
        swap(tmp);
        return *this;
    }

    RedBlackTree& operator=(RedBlackTree&& other) noexcept {
        if (this == &other) return *this;
        clear();
        destroy_recursive(root_);
        ::operator delete(nil_);
        nil_ = other.nil_;
        root_ = other.root_;
        size_ = other.size_;
        comp_ = std::move(other.comp_);
        other.init_nil();
        other.size_ = 0;
        return *this;
    }

    ~RedBlackTree() {
        destroy_recursive(root_);
        ::operator delete(nil_);
    }

    // ---- Capacity ---------------------------------------------------------

    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }
    [[nodiscard]] size_type size() const noexcept { return size_; }
    [[nodiscard]] size_type max_size() const noexcept {
        return std::allocator<Node>().max_size();
    }

    // ---- Lookup -----------------------------------------------------------

    /// Returns 1 if found, 0 if not.
    [[nodiscard]] size_type count(const Key& k) const noexcept {
        return find(k) == end() ? 0 : 1;
    }

    [[nodiscard]] bool contains(const Key& k) const noexcept {
        return find(k) != end();
    }

    iterator find(const Key& k) noexcept {
        Node* n = root_;
        while (n != nil_) {
            if (comp_(k, n->key)) n = n->left;
            else if (comp_(n->key, k)) n = n->right;
            else return iterator(n, this);
        }
        return end();
    }

    const_iterator find(const Key& k) const noexcept {
        Node* n = root_;
        while (n != nil_) {
            if (comp_(k, n->key)) n = n->left;
            else if (comp_(n->key, k)) n = n->right;
            else return const_iterator(n, this);
        }
        return cend();
    }

    iterator lower_bound(const Key& k) noexcept {
        Node* n = root_;
        Node* lb = nil_;
        while (n != nil_) {
            if (!comp_(n->key, k)) {  // n->key >= k
                lb = n;
                n = n->left;
            } else {
                n = n->right;
            }
        }
        if (lb == nil_) return end();
        return iterator(lb, this);
    }

    const_iterator lower_bound(const Key& k) const noexcept {
        Node* n = root_;
        Node* lb = nil_;
        while (n != nil_) {
            if (!comp_(n->key, k)) {
                lb = n;
                n = n->left;
            } else {
                n = n->right;
            }
        }
        if (lb == nil_) return cend();
        return const_iterator(lb, this);
    }

    iterator upper_bound(const Key& k) noexcept {
        Node* n = root_;
        Node* ub = nil_;
        while (n != nil_) {
            if (comp_(k, n->key)) {  // n->key > k
                ub = n;
                n = n->left;
            } else {
                n = n->right;
            }
        }
        if (ub == nil_) return end();
        return iterator(ub, this);
    }

    const_iterator upper_bound(const Key& k) const noexcept {
        Node* n = root_;
        Node* ub = nil_;
        while (n != nil_) {
            if (comp_(k, n->key)) {
                ub = n;
                n = n->left;
            } else {
                n = n->right;
            }
        }
        if (ub == nil_) return cend();
        return const_iterator(ub, this);
    }

    std::pair<iterator, iterator> equal_range(const Key& k) noexcept {
        return {lower_bound(k), upper_bound(k)};
    }
    std::pair<const_iterator, const_iterator> equal_range(const Key& k) const noexcept {
        return {lower_bound(k), upper_bound(k)};
    }

    Value& at(const Key& k) {
        iterator it = find(k);
        if (it == end()) throw std::out_of_range("RedBlackTree::at: key not found");
        return it->second;
    }
    const Value& at(const Key& k) const {
        const_iterator it = find(k);
        if (it == end()) throw std::out_of_range("RedBlackTree::at: key not found");
        return it->second;
    }

    Value& operator[](const Key& k) {
        auto [it, inserted] = insert_unique(k, Value{});
        return it->second;
    }
    Value& operator[](Key&& k) {
        auto [it, inserted] = insert_unique(std::move(k), Value{});
        return it->second;
    }

    // ---- Modifiers --------------------------------------------------------

    /// Returns {iterator, true} on insert, {iterator pointing at existing key, false} otherwise.
    std::pair<iterator, bool> insert(const value_type& kv) {
        return insert_unique(kv.first, kv.second);
    }
    std::pair<iterator, bool> insert(value_type&& kv) {
        return insert_unique(std::move(kv.first), std::move(kv.second));
    }
    template <typename K, typename V>
    std::pair<iterator, bool> insert(K&& k, V&& v) {
        return insert_unique(std::forward<K>(k), std::forward<V>(v));
    }
    /// std::map-style piecewise-construct emplace: builds a (Key, Value)
    /// pair in place to avoid an extra move of either argument.
    template <typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        std::pair<Key, Value> tmp(std::piecewise_construct,
                                  std::forward_as_tuple(std::forward<Args>(args)...),
                                  std::tuple<>{});
        return insert_unique(std::move(tmp.first), std::move(tmp.second));
    }

    size_type erase(const Key& k) {
        iterator it = find(k);
        if (it == end()) return 0;
        erase_node(it.raw_node());
        return 1;
    }

    iterator erase(iterator it) {
        Node* n = it.raw_node();
        ++it;
        erase_node(n);
        return it;
    }
    iterator erase(const_iterator it) {
        Node* n = it.raw_node();
        ++it;
        erase_node(n);
        return iterator(it.raw_node(), this);
    }

    void clear() noexcept {
        destroy_recursive(root_);
        root_ = nil_;
        size_ = 0;
    }

    void swap(RedBlackTree& other) noexcept {
        using std::swap;
        swap(nil_, other.nil_);
        swap(root_, other.root_);
        swap(size_, other.size_);
        swap(comp_, other.comp_);
    }

    // ---- Iteration --------------------------------------------------------

    iterator begin() noexcept {
        if (root_ == nil_) return end();
        return iterator(min_node(root_), this);
    }
    const_iterator begin() const noexcept {
        if (root_ == nil_) return end();
        return const_iterator(min_node(root_), this);
    }
    const_iterator cbegin() const noexcept {
        if (root_ == nil_) return cend();
        return const_iterator(min_node(root_), this);
    }

    iterator end() noexcept { return iterator(nullptr, this); }
    const_iterator end() const noexcept { return const_iterator(nullptr, this); }
    const_iterator cend() const noexcept { return const_iterator(nullptr, this); }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }

    // ---- Verification (test-only helpers) ---------------------------------

    [[nodiscard]] bool verify_invariants() const noexcept {
        if (root_ == nil_) return true;
        if (root_->color != rbt_detail::Color::Black) return false;
        return verify_node(root_);
    }

    [[nodiscard]] const Node* raw_root() const noexcept { return root_; }
    [[nodiscard]] const Node* raw_nil() const noexcept { return nil_; }
    [[nodiscard]] const Compare& comparator() const noexcept { return comp_; }

   private:
    void init_nil() {
        // The shared NIL sentinel is allocated once per tree. Its
        // left/right/parent all point back to itself, so the standard
        // CLRS-style "is this a sentinel?" check (`n == nil_`) works
        // uniformly and any field read on the sentinel returns the
        // sentinel (or its Black colour), which is what rotation,
        // transplant, and erase_fixup rely on. The dedicated iterator
        // helpers below special-case the sentinel to avoid the
        // self-loop turning into a walk past the end.
        nil_ = allocate_node(Key{}, Value{}, rbt_detail::Color::Black, nullptr, nullptr,
                             nullptr);
        nil_->left = nil_;
        nil_->right = nil_;
        nil_->parent = nil_;
        root_ = nil_;
    }

    template <typename K, typename V>
    Node* allocate_node(K&& k, V&& v, rbt_detail::Color c, Node* p, Node* l, Node* r) {
        Node* n = static_cast<Node*>(::operator new(sizeof(Node)));
        try {
            ::new (n) Node(std::forward<K>(k), std::forward<V>(v), c, p, l, r);
        } catch (...) {
            ::operator delete(n);
            throw;
        }
        return n;
    }

    void destroy_node(Node* n) noexcept {
        if (n == nil_) return;  // sentinel owned by class
        n->~Node();
        ::operator delete(n);
    }

    void destroy_recursive(Node* n) noexcept {
        if (n == nil_) return;
        destroy_recursive(n->left);
        destroy_recursive(n->right);
        destroy_node(n);
    }

    template <typename K, typename V>
    std::pair<iterator, bool> insert_unique(K&& k, V&& v) {
        Node* parent = nil_;
        Node* n = root_;
        bool go_left = false;
        while (n != nil_) {
            parent = n;
            if (comp_(k, n->key)) {
                go_left = true;
                n = n->left;
            } else if (comp_(n->key, k)) {
                go_left = false;
                n = n->right;
            } else {
                return {iterator(n, this), false};
            }
        }
        Node* child = allocate_node(std::forward<K>(k), std::forward<V>(v),
                                    rbt_detail::Color::Red, parent, nil_, nil_);
        if (parent == nil_) {
            root_ = child;
        } else if (go_left) {
            parent->left = child;
        } else {
            parent->right = child;
        }
        insert_fixup(child);
        ++size_;
        return {iterator(child, this), true};
    }

    void insert_fixup(Node* z) {
        while (z->parent->color == rbt_detail::Color::Red) {
            if (z->parent == z->parent->parent->left) {
                Node* uncle = z->parent->parent->right;
                if (uncle->color == rbt_detail::Color::Red) {
                    // Case 1: parent + uncle both red.
                    z->parent->color = rbt_detail::Color::Black;
                    uncle->color = rbt_detail::Color::Black;
                    z->parent->parent->color = rbt_detail::Color::Red;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->right) {
                        // Case 2: parent red, uncle black, z is inner.
                        z = z->parent;
                        rotate_left(z);
                    }
                    // Case 3: parent red, uncle black, z is outer.
                    z->parent->color = rbt_detail::Color::Black;
                    z->parent->parent->color = rbt_detail::Color::Red;
                    rotate_right(z->parent->parent);
                }
            } else {
                Node* uncle = z->parent->parent->left;
                if (uncle->color == rbt_detail::Color::Red) {
                    z->parent->color = rbt_detail::Color::Black;
                    uncle->color = rbt_detail::Color::Black;
                    z->parent->parent->color = rbt_detail::Color::Red;
                    z = z->parent->parent;
                } else {
                    if (z == z->parent->left) {
                        z = z->parent;
                        rotate_right(z);
                    }
                    z->parent->color = rbt_detail::Color::Black;
                    z->parent->parent->color = rbt_detail::Color::Red;
                    rotate_left(z->parent->parent);
                }
            }
        }
        root_->color = rbt_detail::Color::Black;
    }

    void rotate_left(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left != nil_) y->left->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_) {
            root_ = y;
        } else if (x == x->parent->left) {
            x->parent->left = y;
        } else {
            x->parent->right = y;
        }
        y->left = x;
        x->parent = y;
    }

    void rotate_right(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right != nil_) y->right->parent = x;
        y->parent = x->parent;
        if (x->parent == nil_) {
            root_ = y;
        } else if (x == x->parent->right) {
            x->parent->right = y;
        } else {
            x->parent->left = y;
        }
        y->right = x;
        x->parent = y;
    }

    /// Transplant u with v, fixing u->parent's child pointer.
    void transplant(Node* u, Node* v) {
        if (u->parent == nil_) {
            root_ = v;
        } else if (u == u->parent->left) {
            u->parent->left = v;
        } else {
            u->parent->right = v;
        }
        v->parent = u->parent;  // safe even if v == nil_
    }

    /// Minimum node in subtree rooted at n. Returns nil_ for empty.
    /// NB: `n` may BE the sentinel -- in that case the function
    /// returns nil_ directly without walking the self-loop.
    Node* min_node(Node* n) const noexcept {
        if (n == nil_) return nil_;
        Node* cur = n;
        while (cur->left != nil_) cur = cur->left;
        return cur;
    }
    Node* max_node(Node* n) const noexcept {
        if (n == nil_) return nil_;
        Node* cur = n;
        while (cur->right != nil_) cur = cur->right;
        return cur;
    }
    /// In-order successor of `n` in the tree rooted at root_. Returns
    /// nil_ when there is no successor. The caller converts nil_ to
    /// nullptr to represent end().
    Node* next_node_in_tree(Node* n) const noexcept {
        if (n == nil_) return nil_;
        if (n->right != nil_) {
            Node* cur = n->right;
            while (cur->left != nil_) cur = cur->left;
            return cur;
        }
        Node* p = n->parent;
        while (p != nil_ && n == p->right) {
            n = p;
            p = p->parent;
        }
        return p;  // may be nil_ if n was the max
    }
    /// In-order predecessor. Mirror of next_node_in_tree.
    Node* prev_node_in_tree(Node* n) const noexcept {
        if (n == nil_) return nil_;
        if (n->left != nil_) {
            Node* cur = n->left;
            while (cur->right != nil_) cur = cur->right;
            return cur;
        }
        Node* p = n->parent;
        while (p != nil_ && n == p->left) {
            n = p;
            p = p->parent;
        }
        return p;  // may be nil_ if n was the min
    }
    Node* next_node(Node* n) const noexcept {
        if (n == nil_) return nullptr;
        Node* res = next_node_in_tree(n);
        return res == nil_ ? nullptr : res;
    }
    Node* prev_node(Node* n) const noexcept {
        if (n == nil_) return nullptr;
        Node* res = prev_node_in_tree(n);
        return res == nil_ ? nullptr : res;
    }

    void erase_node(Node* z) {
        Node* y = z;
        Node* x = nil_;
        rbt_detail::Color y_orig_color = y->color;
        if (z->left == nil_) {
            x = z->right;
            transplant(z, z->right);
        } else if (z->right == nil_) {
            x = z->left;
            transplant(z, z->left);
        } else {
            // y = successor of z (min of z->right subtree)
            y = z->right;
            while (y->left != nil_) y = y->left;
            y_orig_color = y->color;
            x = y->right;
            if (y->parent == z) {
                x->parent = y;  // even if x is nil_, ok for the fixup
            } else {
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }
        if (y_orig_color == rbt_detail::Color::Black) erase_fixup(x);
        destroy_node(z);
        --size_;
    }

    void erase_fixup(Node* x) {
        while (x != root_ && x->color == rbt_detail::Color::Black) {
            if (x == x->parent->left) {
                Node* w = x->parent->right;
                if (w->color == rbt_detail::Color::Red) {
                    // Case 1: sibling red.
                    w->color = rbt_detail::Color::Black;
                    x->parent->color = rbt_detail::Color::Red;
                    rotate_left(x->parent);
                    w = x->parent->right;
                }
                if (w->left->color == rbt_detail::Color::Black &&
                    w->right->color == rbt_detail::Color::Black) {
                    // Case 2: sibling black, both nephews black.
                    w->color = rbt_detail::Color::Red;
                    x = x->parent;
                } else {
                    if (w->right->color == rbt_detail::Color::Black) {
                        // Case 3: sibling black, near nephew red, far black.
                        w->left->color = rbt_detail::Color::Black;
                        w->color = rbt_detail::Color::Red;
                        rotate_right(w);
                        w = x->parent->right;
                    }
                    // Case 4: sibling black, far nephew red.
                    w->color = x->parent->color;
                    x->parent->color = rbt_detail::Color::Black;
                    w->right->color = rbt_detail::Color::Black;
                    rotate_left(x->parent);
                    x = root_;
                }
            } else {
                Node* w = x->parent->left;
                if (w->color == rbt_detail::Color::Red) {
                    w->color = rbt_detail::Color::Black;
                    x->parent->color = rbt_detail::Color::Red;
                    rotate_right(x->parent);
                    w = x->parent->left;
                }
                if (w->right->color == rbt_detail::Color::Black &&
                    w->left->color == rbt_detail::Color::Black) {
                    w->color = rbt_detail::Color::Red;
                    x = x->parent;
                } else {
                    if (w->left->color == rbt_detail::Color::Black) {
                        w->right->color = rbt_detail::Color::Black;
                        w->color = rbt_detail::Color::Red;
                        rotate_left(w);
                        w = x->parent->left;
                    }
                    w->color = x->parent->color;
                    x->parent->color = rbt_detail::Color::Black;
                    w->left->color = rbt_detail::Color::Black;
                    rotate_right(x->parent);
                    x = root_;
                }
            }
        }
        x->color = rbt_detail::Color::Black;
    }

[[nodiscard]] bool verify_node(Node* n) const noexcept {
        if (n == nil_) return true;
        // No double-red.
        if (n->color == rbt_detail::Color::Red) {
            if (n->left->color == rbt_detail::Color::Red) return false;
            if (n->right->color == rbt_detail::Color::Red) return false;
        }
        // BST order: left subtree < n, n < right subtree.
        if (n->left != nil_ && !comp_(n->left->key, n->key)) return false;
        if (n->right != nil_ && !comp_(n->key, n->right->key)) return false;
        // Parent pointers (only for non-nil children -- nil is a shared
        // sentinel and its parent is meaningless for verification).
        if (n->left != nil_ && n->left->parent != n) return false;
        if (n->right != nil_ && n->right->parent != n) return false;
        return verify_node(n->left) && verify_node(n->right);
    }
};

template <typename K, typename V, typename C>
inline void swap(RedBlackTree<K, V, C>& a, RedBlackTree<K, V, C>& b) noexcept {
    a.swap(b);
}

}  // namespace imdb
