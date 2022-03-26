// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALIGNED_ALLOC_H_
#define BASE_ALIGNED_ALLOC_H_

namespace base {

#ifdef _WIN32
// The Microsoft compiler does not support std::aligned_alloc
// but it provides _aligned_malloc and _aligned_free
#include <malloc.h>
static inline void* aligned_alloc(size_t alignment, size_t size) {
  return _aligned_malloc(size, alignment);
}

static inline void aligned_free(void* block) {
  _aligned_free(block);
}

#elif defined(__APPLE__)
// On MacOS, std::aligned_alloc() is only available with the most
// recent releases of the system only, so provide a fallback to
// run the compiled code on any OS X release.
void* aligned_alloc(size_t alignment, size_t size);
void aligned_free(void* block);
#else  // !_WIN32
#include <cstdlib>
static inline void* aligned_alloc(size_t alignment, size_t size) {
  return std::aligned_alloc(alignment, size);
}

static inline void aligned_free(void* block) {
  std::free(block);
}
#endif  // !_WIN32

}  // namespace base

#endif  // BASE_ALIGNED_ALLOC_H_
