// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_UTIL_WIN_H_
#define BASE_STRINGS_STRING_UTIL_WIN_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "base/logging.h"

namespace base {

inline int vsnprintf(char* buffer,
                     size_t size,
                     const char* format,
                     va_list arguments) {
  // Use the C99 vsnprintf, which returns the number of characters that would
  // have been written (even on truncation) and always null-terminates --
  // exactly base::vsnprintf's contract. Under MinGW this resolves to the
  // libmingwex implementation (via -D__USE_MINGW_ANSI_STDIO=1), avoiding the
  // MSVC "secure" vsnprintf_s/_vscprintf that XP's msvcrt.dll does not export.
  return ::vsnprintf(buffer, size, format, arguments);
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_UTIL_WIN_H_
