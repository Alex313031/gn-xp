// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/aligned_alloc.h"

namespace base {

void* aligned_alloc(size_t alignment, size_t size) {
  if (alignment < alignof(void*))
    alignment = alignof(void*);

  // Allocate block and store its address just before just before the
  // result's address, as in:
  //    ________________________________________
  //   |    |          |                        |
  //   |    | real_ptr |                        |
  //   |____|__________|________________________|
  //
  //   ^               ^
  //   real_ptr         result
  //
  void* real_ptr = ::malloc(size + sizeof(void*) + alignment - 1);

  // Compute the result's address, add sizeof(void*) + padding.
  auto addr = reinterpret_cast<uintptr_t>(real_ptr) + sizeof(void*);
  uintptr_t padding = (alignment - addr) % alignment;
  addr += padding;

  // store the real address just before the result.
  reinterpret_cast<void**>(addr - sizeof(void*))[0] = real_ptr;
  return reinterpret_cast<void*>(addr);
}

void aligned_free(void* block) {
  if (block) {
    // Retrieve the address of the real pointer just before the block.
    auto* real_ptr = reinterpret_cast<void**>(block) - 1;
    std::free(real_ptr);
  }
}

}  // namespace base
