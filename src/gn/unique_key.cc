// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/unique_key.h"

#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/containers/flat_set.h"

namespace {

// Implementation note:
//
// UniqueStrings implements the global shared state, which is:
//
//    - a group of std::string instances with a persistent addres, allocated
//      through a fast slab allocator.
//
//    - a set of string pointers, corresponding to the known strings in the
//      group.
//
//    - a mutex to ensure correct thread-safety.
//
//    - a find() method that takes an std::string_view argument, and uses it
//      to find a matching entry in the string tree. If none is available,
//      a new std::string is allocated and its address inserted into the tree
//      before being returned.
//
// Because the mutex is a large bottleneck, each thread implements
// its own local string pointer cache, and will only call UniqueString::find()
// in case of a lookup miss. This is critical for good performance.
//

static const std::string kEmptyString;

using KeyType = const std::string*;

// Implementation for the thread-local string pointer cache and the global one
// as well.
//
// This is a trivial hash table using array addressing. It is faster in
// practice than using a standard container or even a base::flat_set<>.
//
struct FastKeySet {
  struct Node {
    size_t hash = 0;
    KeyType key = nullptr;
  };

  // Compute hash for |str|. Replace with faster hash function if available.
  static size_t Hash(std::string_view str) {
    return std::hash<std::string_view>()(str);
  }

  // Lookup for |str| with specific |hash| value.
  // Return a Node pointer. If the key was found, |node.key| is its value.
  // Otherwise, the caller should create a new key value, then call Insert()
  // below.
  Node* Lookup(size_t hash, std::string_view str) const {
    size_t index = hash & (size_ - 1);
    Node* nodes = const_cast<Node*>(&buckets_[0]);
    Node* nodes_limit = nodes + size_;
    Node* node = nodes + index;
    for (;;) {
      if (!node->key)
        return node;
      if (node->hash == hash && node->key[0] == str)
        return node;
      if (++node == nodes_limit)
        node = nodes;
    }
  }

  // Insert a new key in this set. |node| must be a value returned by
  // a previous Lookup() call. |hash| is the hash value for |key|.
  void Insert(Node* node, size_t hash, KeyType key) {
    node->hash = hash;
    node->key = key;
    count_ += 1;
    if (count_ * 4 >= size_ * 3)  // 75% max load factor
      GrowBuckets();
  }

  void GrowBuckets() {
    size_t size = buckets_.size();
    size_t new_size = size * 2;
    size_t new_mask = new_size - 1;

    std::vector<Node> new_buckets;
    new_buckets.resize(new_size);
    for (const Node& node : buckets_) {
      size_t index = node.hash & new_mask;
      for (;;) {
        Node& node2 = new_buckets[index];
        if (!node2.key) {
          node2 = node;
          break;
        }
        index = (index + 1) & new_mask;
      }
    }
    buckets_ = std::move(new_buckets);
    size_ = new_size;
  }

  size_t size_ = 2;
  size_t count_ = 0;
  std::vector<Node> buckets_ = {Node{}, Node{}};
};

struct UniqueStrings {
  static constexpr unsigned kStringsPerSlab = 128;
  union StringStorage {
    StringStorage(){};
    ~StringStorage() {}
    char dummy;
    std::string str;
  };
  struct Slab {
    Slab() {}
    StringStorage items[kStringsPerSlab];
    std::string* get(size_t index) { return &items[index].str; }
  };

  UniqueStrings() {
    // Ensure kEmptyString is in our set while not being allocated
    // from a slab. The end result is that find("") should always
    // return this address.
    size_t hash = set_.Hash("");
    auto node = set_.Lookup(hash, "");
    set_.Insert(node, hash, &kEmptyString);
  }

  // Thread-safe lookup function.
  const std::string* find(std::string_view key) {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t hash = set_.Hash(key);
    auto node = set_.Lookup(hash, key);
    if (node->key)
      return node->key;

    // Allocate new string, insert its address in the set.
    if (slab_index_ >= kStringsPerSlab) {
      slabs_.push_back(new Slab);
      slab_index_ = 0;
    }
    std::string* result = slabs_.end()[-1]->get(slab_index_++);
    new (result) std::string(key);
    set_.Insert(node, hash, result);
    return result;
  }

 private:
  std::mutex mutex_;
  FastKeySet set_;
  std::vector<Slab*> slabs_;
  unsigned slab_index_ = kStringsPerSlab;
};

UniqueStrings& GetUniqueStrings() {
  static UniqueStrings s_unique_string_set;
  return s_unique_string_set;
}

// Each thread maintains its own ThreadLocalCache to perform fast lookups
// without taking any mutex in most cases.
struct ThreadLocalCache {
  KeyType find(std::string_view key) {
    size_t hash = local_set_.Hash(key);
    auto node = local_set_.Lookup(hash, key);
    if (node->key)
      return node->key;

    KeyType result = GetUniqueStrings().find(key);
    local_set_.Insert(node, hash, result);
    return result;
  }

  FastKeySet local_set_;
};

thread_local ThreadLocalCache s_local_cache;

}  // namespace

UniqueKey::UniqueKey() : value_(kEmptyString) {}

UniqueKey::UniqueKey(std::string_view str) noexcept
    : value_(*s_local_cache.find(str)) {}
