// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NESTED_SET_H_
#define TOOLS_GN_NESTED_SET_H_

#include "base/atomic_ref_count.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "gn/unique_vector.h"

#include <atomic>
#include <vector>

// Indicates in which order items should be returned when calling
// the NestedSet<T>::Flatten() method.
//
// - Default indicates that the order is not important
//   as long as it is stable. Fast to compute. This currently performs a
//   breadth-first walk over the DAG, but clients should not rely on that.
//   This is useful if the result is going to be sorted afterwards.
//
// - Include indicates that items from dependencies must
//   appear before items from dependents. Left-to-right ordering of
//   items and dependencies is preserved otherwise. This is typically the
//   case with include directories during compilation. Implemented with
//   a left-to-right post-order traverasl over the DAG.
//
// - Link indicates that items from dependencies must
//   appear after items from dependents. Left-to-right ordering of
//   items and dependencies is preserved otherwised. This is typically the
//   case for libraries passed to the linker when generating executables.
//   Implemented with a right-to-left postorder traversal then reversing
//   the result to accumulate all items of each header left-to-right.
//
// - Legacy is a naive left-to-right pre-order traversal over the DAG
//   that tries to respect left-to-right ordering over dependency order.
//   This corresponds to what GN was doing before NestedSet<> were
//   introduced,
//
// See examples (with graphics) in the unit tests to better
// undertand the differences.
//
enum class NestedSetOrder {
  Default,
  Include,
  Link,
  Legacy,
};

// A NestedSet implements a scoped pointer to a referenced-counted
// immutable ordered set of items of type T. The implementation is
// tailored for fast merge operations, while flattenning, i.e.
// collecting list of unique items in the set, is more expensive.
//
// Usage is the following:
//
//   1) Use NestedSet<T>() to create an instance of the empty set.
//
//   2) Otherwise, create a NestedPointer<T>::Builder object then
//      call its AddItem() method to add a direct item to the set,
//      or AddDep() to add a transitive dependency (this is a fast
//      operation but conceptually adds all items reached transitively
//      from the dependency to the set being constructed). Repeat
//      as many times as needed, then call the Build() method to
//      retrieve a new NestedSet<T> instance.
//
//   3) A NestedSet<T> is really a reference to a thread-safe
//      ref-counted object, so can be copied or moved efficiently.
//
//   4) Call NestedSet<T>::Flatten() or NestedSet<T>::Flatten(order)
//      in order to convert all items in the set to a vector of unique
//      items. Note that the order of items in the result is controlled
//      by the |order| argument. See the NestedSetOrder documentation
//      for details.
//
//   5) Call NestedSet<T>::Contain() to verify if an item belongs
//      to a set. Note that this operation is O(N). To perform fast
//      repeated lookups, flatten the set and build a base::flat_set<>
//      from the result, which will provide O(logN) lookup performance.
//      This is not generally needed by GN in its critical path.
//
template <typename T>
class NestedSet {
  // A NestedSet is implemented as a small header, followed by
  // |num_deps| pointers to dependencies, and |num_items| items of type T.
  // Each instance is reference-counted. Use AddRef() to increment the count
  // and Unref() to decrement it, and potentially destroy the instance.
  struct Header {
    Header(size_t deps_count, size_t items_count)
        : ref_count(0),
          num_deps(static_cast<uint32_t>(deps_count)),
          num_items(static_cast<uint32_t>(items_count)) {
      // Check for 32-bit overflows.
      DCHECK(num_deps == deps_count);
      DCHECK(num_items == items_count);
    }

    base::AtomicRefCount ref_count;
    uint32_t num_deps;
    uint32_t num_items;

    // Return the offset of dependencies relative to the start of the
    // header in bytes.
    size_t deps_offset() const {
      size_t result = sizeof(Header);
      size_t padding = (alignof(Header*) - result) % alignof(Header*);
      return result + padding;
    }

    // Return the offset of items relative to the start of the header
    // in bytes.
    size_t items_offset() const {
      size_t result = deps_offset() + num_deps * sizeof(Header*);
      size_t padding = (alignof(T) - result) % alignof(T);
      return result + padding;
    }

    // Return the total allocation size for a given header in bytes.
    size_t allocation_size() const {
      return items_offset() + num_items * sizeof(T);
    }

    // Return the byte alignment for Header instances.
    static size_t alignment() { return std::max(alignof(T), alignof(Header*)); }

    // Return pointer to the first pointer to a header dependency.
    Header* const* deps_begin() const {
      return reinterpret_cast<Header* const*>(
          reinterpret_cast<const char*>(this) + deps_offset());
    }

    // Return pointer after the last header dependency pointer.
    Header* const* deps_end() const { return deps_begin() + num_deps; }

    // Return a span covering the header dependency pointers in this instance.
    base::span<Header* const> deps() const {
      return base::make_span(deps_begin(), num_deps);
    }

    // Return pointer to first item.
    const T* items_begin() const {
      return reinterpret_cast<const T*>(reinterpret_cast<const char*>(this) +
                                        items_offset());
    }

    // Return pointer after last item.
    const T* items_end() const { return items_begin() + num_items; }

    // Return a span covering the items in this instance.
    base::span<const T> items() const {
      return base::make_span(items_begin(), num_items);
    }

    // Increment the reference count for |header| is not null,
    // then return input pointer.
    static Header* AddRef(Header* header) {
      if (header)
        header->ref_count.Increment();
      return header;
    }

    // Decrement the reference count for |header| if not null.
    // If the count reaches 0, this deallocates the header,
    // decrementing the count of its dependencies recursively.
    static void Unref(Header* header) {
      if (header && !header->ref_count.Decrement())
        Deallocate(header);
    }

    // Allocate a new header for |num_deps| dependencies and
    // |num_items| items. Return its address after populating
    // the header struct, but not the following dependencies
    // and items arrays.
    static Header* Allocate(size_t num_deps, size_t num_items) {
      if (num_deps == 0 && num_items == 0)
        return nullptr;

      Header header(num_deps, num_items);

      Header* result = reinterpret_cast<Header*>(
          std::aligned_alloc(header.alignment(), header.allocation_size()));

      new (result) Header(num_deps, num_items);
      return result;
    }

    // Deallocate current instance, ensuring that this unrefs
    // all dependencies recursively before releasing memory.
    static void Deallocate(Header* from) {
      base::queue<Header*> sets;
      sets.push(from);
      while (!sets.empty()) {
        Header* current = sets.front();
        sets.pop();
        DCHECK(current->ref_count.IsZero());
        DCHECK(current->num_deps > 0 || current->num_items > 0);
        for (Header* dep : current->deps()) {
          DCHECK(!dep->ref_count.IsZero());
          if (!dep->ref_count.Decrement())
            sets.push(dep);
        }
        // Destroy items in place.
        for (auto& item : current->items())
          item.~T();

        std::free(current);
      }
    }
  };

  // A NestedSet instance is just an owning pointer to a Header.
  Header* header_ = nullptr;

  explicit NestedSet(Header* header) : header_(Header::AddRef(header)) {}

  // Used by Flatten() to implement a stack for depth-first-search walk.

  struct VisitSlot {
    VisitSlot(const Header* header)
        : header(header),
          dep_current(header->deps_begin()),
          dep_limit(header->deps_end()) {}

    const Header* header;
    const Header* const* dep_current;
    const Header* const* dep_limit;
  };

  using VisitStack = std::vector<VisitSlot>;

 public:
  // Default constructor. This returns the empty set instance.
  NestedSet() = default;

  // Destructor.
  ~NestedSet() { Header::Unref(header_); }

  // Copy operations
  NestedSet(const NestedSet& other) : header_(Header::AddRef(other.header_)) {}
  NestedSet& operator=(const NestedSet& other) {
    if (this != &other) {
      this->~NestedSet();
      new (this) NestedSet(other);
    }
    return *this;
  }

  // Move operations
  NestedSet(NestedSet&& other) noexcept : header_(other.header_) {
    other.header_ = nullptr;
  }
  NestedSet& operator=(NestedSet&& other) noexcept {
    if (this != &other) {
      this->~NestedSet();
      new (this) NestedSet(std::move(other));
    }
    return *this;
  }

  // Returns true if this set is empty, false otherwise.
  bool empty() const {
    if (!header_)
      return true;
    DCHECK(header_->num_deps > 0 || header_->num_items > 0);
    return false;
  }

  // Return number of direct dependencies. Only useful for debugging.
  size_t deps_count() const { return header_ ? header_->num_deps : 0; };

  // Return a vector of direct dependencies. Only useful for debugging.
  std::vector<NestedSet> deps() const {
    std::vector<NestedSet> result;
    if (header_) {
      result.reserve(header_->num_deps);
      for (auto* dep : header_->deps()) {
        result.push_back(NestedSet(dep));
      }
    }
    return result;
  }

  // Return the reference count for the associated header. Only usable
  // in unit tests.
  int ref_count() const {
    return header_ ? header_->ref_count.SubtleRefCountForDebug() : 0;
  }

  // Return number of direct items. Only useful for debugging. To get
  // all items from the set, use Flatten() instead.
  size_t items_count() const { return header_ ? header_->num_items : 0; };

  // Return a span of direct items for the set (excluding those from
  // dependencies). Only useful for debugging.
  base::span<const T> items() const {
    return header_ ? header_->items() : base::span<const T>{};
  }

  bool operator==(const NestedSet& other) const {
    return header_ == other.header_;
  }

  bool operator!=(const NestedSet& other) const { return !(*this == other); }

  // A helper class used to build NestedSet instances. Usage is:
  //  1) Create NestedSet<T>::Builder instance.
  //  2) Call AddItem() or AddDep() as many times as necessary.
  //  3) Call Build() to return the corresponding NestedSet.
  //
  class Builder {
   public:
    Builder& AddDep(NestedSet dep) {
      if (!dep.empty()) {
        deps_.push_back(std::move(dep));
      }
      return *this;
    }

    Builder& AddItem(const T& item) {
      items_.push_back(item);
      return *this;
    }

    Builder& AddItem(T&& item) {
      items_.push_back(std::move(item));
      return *this;
    }

    Builder& AddItems(base::span<const T> items) {
      for (const auto& item : items)
        items_.push_back(item);
      return *this;
    }

    Builder& Reset() {
      deps_.clear();
      items_.clear();
      return *this;
    }

    NestedSet Build() const {
      if (items_.empty()) {
        if (deps_.empty()) {
          // Return the empty set.
          return {};
        }
        if (deps_.size() == 1) {
          // This only contains a reference to a single set,
          // so return it directly.
          return deps_[0];
        }
      }

      // Verify that no dependency is empty.
      if (DCHECK_IS_ON()) {
        for (const NestedSet& dep : deps_)
          DCHECK(!dep.empty());
      }

      Header* header = Header::Allocate(deps_.size(), items_.size());

      // Copy dependency pointers.
      if (!deps_.empty()) {
        Header** dst = const_cast<Header**>(header->deps_begin());
        for (auto& src_dep : deps_)
          *dst++ = Header::AddRef(src_dep.header_);
      }

      // Copy-construct items
      if (!items_.empty()) {
        T* dst = const_cast<T*>(header->items_begin());
        for (const auto& src_item : items_) {
          new (dst) T(src_item);
          dst++;
        }
      }

      DCHECK(header && header->ref_count.IsZero());
      return NestedSet(header);
    }

   private:
    std::vector<NestedSet> deps_;
    std::vector<T> items_;
  };

  // Retrieves all items from the set (including those from transitive
  // dependencies). Each item only appears once, and the order of items
  // is controlled by the |order| argument.
  std::vector<T> Flatten(NestedSetOrder order = NestedSetOrder::Default) const {
    if (!header_)
      return {};

    UniqueVector<T> unique;
    UniqueVector<const Header*> visited;
    visited.push_back(header_);

    switch (order) {
      case NestedSetOrder::Include: {
        // Dependencies appear before dependents in the output.
        // Left-to-right ordering is preserved if possible. E.g.:
        // {A, B, {C, D}, {C, E}} -> D E C A B
        // Implemented with a Depth-First-Search walk over the DAG.
        VisitStack stack;
        stack.push_back(header_);
        while (!stack.empty()) {
          auto& cur = stack.back();
          if (cur.dep_current != cur.dep_limit) {
            const Header* dep = *cur.dep_current++;
            if (visited.push_back(dep)) {
              stack.push_back(dep);
            }
          } else {
            for (const auto& item : cur.header->items())
              unique.push_back(item);
            stack.pop_back();
          }
        }
        break;
      }
      case NestedSetOrder::Link: {
        // Dependencies appear after their dependents in the output.
        // Left-to-right ordering is preserved if possible.
        // Record all nodes visited using a post-order DFS walk, where
        // dependencies are visited in right-to-left for each node,
        // then collect the items from the reversed list of collected
        // nodes.
        std::vector<const Header*> postorder;
        VisitStack stack;
        stack.push_back(header_);
        while (!stack.empty()) {
          auto& cur = stack.back();
          if (cur.dep_current != cur.dep_limit) {
            const Header* dep = *--cur.dep_limit;
            if (visited.push_back(dep)) {
              stack.push_back(dep);
            }
          } else {
            postorder.push_back(cur.header);
            stack.pop_back();
          }
        }
        auto it = postorder.end();
        while (it != postorder.begin()) {
          --it;
          for (const auto& item : (*it)->items())
            unique.push_back(item);
        }
        break;
      }
      case NestedSetOrder::Legacy: {
        // Naive pre-order depth-first search.
        VisitStack stack;
        stack.push_back(header_);
        for (const auto& item : header_->items())
          unique.push_back(item);
        while (!stack.empty()) {
          auto& cur = stack.back();
          if (cur.dep_current != cur.dep_limit) {
            const Header* dep = *cur.dep_current++;
            if (visited.push_back(dep)) {
              for (const auto& item : dep->items())
                unique.push_back(item);
              stack.push_back(dep);
            }
          } else {
            stack.pop_back();
          }
        }
        break;
      }
      default: {  // NestedSetOrder::Default
        // Whether a DFS or BFS is faster depends on the structure
        // of the DAG being traversed. For now assume that GN's
        // targets graph are very deep, unbalanced with relatively
        // small number of dependencies per target on average, and use a
        // BFS. Revisit is profiling shows otherwise.
        base::queue<const Header*> queue;
        queue.push(header_);
        while (!queue.empty()) {
          const Header* current = queue.front();
          queue.pop();
          for (const auto& item : current->items())
            unique.push_back(item);
          for (const Header* dep : current->deps()) {
            if (visited.push_back(dep))
              queue.push(dep);
          }
        }
      }
    }

    return unique.release();
  }

  // Return true if the set contains |item|. Note that this is O(N).
  bool Contains(const T& item) const {
    if (!header_)
      return false;

    UniqueVector<const Header*> visited;
    visited.push_back(header_);
    base::queue<const Header*> queue;
    queue.push(header_);
    while (!queue.empty()) {
      const Header* cur = queue.front();
      queue.pop();
      for (const auto& cur_item : cur->items()) {
        if (cur_item == item)
          return true;
      }
      for (const auto* dep : cur->deps()) {
        if (visited.push_back(dep))
          queue.push(dep);
      }
    }
    return false;
  }

  // A very inefficient way to insert in a NestedSet instance,
  // Only use this in tests.
  void AddForTest(const T& item) {
    NestedSet temp = std::move(*this);
    *this = Builder().AddItem(item).AddDep(std::move(temp)).Build();
  }
};

#endif  // TOOLS_GN_NESTED_SET_H_
