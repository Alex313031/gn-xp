#ifndef SOURCE_GN_SRC_GN_POINTER_SET_H_
#define SOURCE_GN_SRC_GN_POINTER_SET_H_

#include <functional>
#include "gn/hash_table_base.h"

// PointerSet<T> is a fast implemention of a set of non-owning typed
// pointer values (of type T*).

struct PointerSetNode {
  const void* ptr_;
  bool is_null() const { return !ptr_; }
  bool is_tombstone() const { return reinterpret_cast<uintptr_t>(ptr_) == 1; }
  bool is_valid() const { return !is_null() && !is_tombstone(); }
  size_t hash_value() const { return MakeHash(ptr_); }
  static const void* MakeTombstone() {
    return reinterpret_cast<const void*>(1u);
  }
  static size_t MakeHash(const void* ptr) {
    return std::hash<const void*>()(ptr);
  }
};

template <typename T>
class PointerSet : public HashTableBase<PointerSetNode> {
 public:
  using NodeType = PointerSetNode;
  using BaseType = HashTableBase<NodeType>;

  PointerSet() = default;

  PointerSet(const PointerSet& other) { insert(other); }

  PointerSet(PointerSet&& other) noexcept : BaseType(std::move(other)) {}

  // Range constructor.
  template <typename InputIter>
  PointerSet(InputIter first, InputIter last) {
    for (; first != last; ++first)
      add(*first);
  }

  void clear() { NodeClear(); }

  bool add(T* item) {
    NodeType* node = Lookup(item);
    if (node->is_valid())
      return false;

    node->ptr_ = item;
    UpdateAfterInsert();
    return true;
  }

  bool contains(T* item) const { return Lookup(item)->is_valid(); }

  bool erase(T* item) {
    NodeType* node = Lookup(item);
    if (!node->is_valid())
      return false;
    node->ptr_ = node->MakeTombstone();
    UpdateAfterRemoval();
    return true;
  }

  void insert(T* item) { add(item); }

  template <typename InputIter>
  void insert(InputIter first, InputIter last) {
    for (; first != last; ++first)
      add(*first);
  }

  void insert(const PointerSet& other) {
    for (const_iterator iter = other.begin(); iter.valid(); ++iter)
      add(*iter);
  }

  PointerSet intersection_with(const PointerSet& other) const {
    PointerSet result;
    for (const_iterator iter = other.begin(); iter.valid(); ++iter) {
      if (contains(*iter))
        result.add(*iter);
    }
    return result;
  }

  // Only provide const interators for pointer sets.
  struct const_iterator {
    T* const* operator->() const {
      return &const_cast<T*>(static_cast<const T*>(iter_->ptr_));
    }
    T* operator*() const {
      return const_cast<T*>(static_cast<const T*>(iter_->ptr_));
    }
    bool operator==(const const_iterator& other) const {
      return iter_ == other.iter_;
    }
    bool operator!=(const const_iterator& other) const {
      return iter_ != other.iter_;
    }
    bool valid() const { return iter_.valid(); }

    const_iterator& operator++() {
      iter_++;
      return *this;
    }
    const_iterator operator++(int) {
      const_iterator result = *this;
      ++(*this);
      return result;
    }
    NodeIterator iter_;

    // The following allows:
    // std::vector<T*> vector(set.begin(), set.end());
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T*;
    using pointer = T**;
    using reference = T*&;
  };

  const_iterator begin() const { return {NodeBegin()}; }
  const_iterator end() const { return {NodeEnd()}; }

  // Only used for unit-tests so performance is not critical.
  bool operator==(const PointerSet& other) const {
    if (size() != other.size())
      return false;

    for (const_iterator iter = begin(); iter.valid(); ++iter)
      if (!other.contains(*iter))
        return false;

    for (const_iterator iter = other.begin(); iter.valid(); ++iter)
      if (!contains(*iter))
        return false;

    return true;
  }

 private:
  NodeType* Lookup(T* item) const {
    size_t hash = NodeType::MakeHash(item);
    return NodeLookup(hash, [&](NodeType* node) { return node->ptr_ == item; });
  }
};

#endif  // SOURCE_GN_SRC_GN_POINTER_SET_H_
