// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RESOLVER_H_
#define TOOLS_GN_RESOLVER_H_

#include <bitset>
#include <condition_variable>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <vector>

#include "base/md5.h"
#include "tools/gn/label.h"
#include "tools/gn/target.h"

// chunk_size is assumed to be a multiple of max_depth and depth is assumed to
// start at zero.
template <size_t depth, size_t max_depth, size_t chunk_size>
struct Node {
  static constexpr size_t child_size = 1 << chunk_size;
  static constexpr size_t mask = (1 << chunk_size) - 1;
  using Hash = uint64_t;
  using ChildNode = Node<depth + chunk_size, max_depth, chunk_size>;
  std::shared_future<ChildNode&> children[child_size];
  std::unique_ptr<ChildNode> owner[child_size];
  std::atomic_bool has_leaf[child_size];
  Node() {
    for (int i = 0; i < child_size; ++i) {
      children[i] =
          std::async(std::launch::deferred, [this, i]() -> ChildNode& {
            owner[i] = std::make_unique<ChildNode>();
            return *owner[i];
          });
    }
  }
  // TODO: Optimize atomic accesses here.
  void SetTarget(Hash hash, const Target* target, Err* err) {
    auto idx = hash & mask;
    auto& child = children[idx].get();
    child.SetTarget(hash >> chunk_size, target, err);
    has_leaf[idx] = true;
  }

  const Target* GetTarget(Hash hash, Err* err) const {
    auto& child = children[hash & mask].get();
    return child.GetTarget(hash >> chunk_size, err);
  }

  template <class F>
  void foreach (const F& f) const {
    for (int i = 0; i < child_size; ++i) {
      if (has_leaf[i].load())
        children[i].get().foreach (f);
    }
  }
};

template <size_t max_depth, size_t chunk_size>
struct Node<max_depth, max_depth, chunk_size> {
  using Hash = uint64_t;

  mutable std::shared_timed_mutex mutex;
  std::unique_lock<std::shared_timed_mutex> prelock;
  const Target* target_;

  Node() : prelock(this->mutex), target_(nullptr) {}

  void SetTarget(Hash hash, const Target* target, Err* err) {
    if (target_) {
      *err = Err(target->defined_from(), "Target has already been declared.",
                 "You cannot create the same target more than once.");
      // if (target_->defined_from())
      //   err->AppendSubErr(
      //       Err(target_->defined_from(), "Previous declaration was here."));
      return;
    }

    target_ = target;
    prelock.unlock();
  }

  const Target* GetTarget(Hash hash, Err* err) const {
    std::shared_lock<std::shared_timed_mutex> lock(mutex);
    return target_;
  }

  template <class F>
  void foreach (const F& f) const {
    DCHECK(target_);
    f(target_);
  }
};

// TODO: Using an atomic paralell bool array would allow us to implement
// a kind of foreach.
template <size_t bit_size = 64, size_t chunk_size = 1>
class Resolver {
 public:
  using Hash = uint64_t;

  void Set(const Target* target, Err* err) {
    root.SetTarget(HashLabel(target->label()), target, err);
  }

  const Target* Get(Label label, Err* err) const {
    return root.GetTarget(HashLabel(label), err);
  }

  template <class F>
  void foreach (const F& f) {
    root.foreach (f);
  }

 private:
  Hash HashLabel(Label label) const {
    std::string md5 = base::MD5String(label.GetUserVisibleName(true));
    // FIXME(juliehockett): this assumes ascii chars (1 char = 8 bits).
    return std::stoul(md5.substr(0, 8), nullptr, 16);
  }

  Node<0, bit_size, chunk_size> root;
};

#endif  // TOOLS_GN_RESOLVER_H_
