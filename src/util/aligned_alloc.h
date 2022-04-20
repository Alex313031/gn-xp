// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_UTIL_ALIGNED_ALLOC_H_
#define TOOLS_UTIL_ALIGNED_ALLOC_H_

#include <cstdlib>

#ifdef _WIN32
#define USE_STD_ALIGNED_ALLOC 0
#else
#define USE_STD_ALIGNED_ALLOC 1
#endif

#if !USE_STD_ALIGNED_ALLOC
#include <malloc.h>  // for _aligned_malloc() and _aligned_free()
#endif

// AlignedAlloc<N> provides Alloc() and Free() methods that can be
// used to allocate and release blocks of heap memory aligned with
// N bytes.
//
// The implementation uses std::aligned_alloc() when it is available,
// or uses fallbacks otherwise. On Win32, _aligned_malloc() and
// _aligned_free() are used.
template <size_t ALIGNMENT>
struct AlignedAlloc {
  static void* Alloc(size_t size) {
    static_assert((ALIGNMENT & (ALIGNMENT - 1)) == 0,
                  "ALIGNMENT must be a power of 2");
#if USE_STD_ALIGNED_ALLOC
    return std::aligned_alloc(ALIGNMENT, size);
#else  // USE_STD_ALIGNED_ALLOC
    return _aligned_malloc(size, ALIGNMENT);
#endif
  }

  static void Free(void* block) {
#if USE_STD_ALIGNED_ALLOC
    // Allocation came from std::aligned_alloc()
    return std::free(block);
#else  // USE_STD_ALIGNED_ALLOC
    _aligned_free(block);
#endif
  }
};

#endif  // TOOLS_UTIL_ALIGNED_ALLOC_H_
