// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ticks.h"

#include "base/logging.h"
#include "build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <mach/mach_time.h>
#elif defined(OS_POSIX)
#include <time.h>
#else
#error Port.
#endif

#if defined(OS_ZOS)
typedef enum {
  CLOCK_REALTIME,
  CLOCK_MONOTONIC,
  CLOCK_HIGHRES,
  CLOCK_THREAD_CPUTIME_ID
} clockid_t;

static int clock_gettime(clockid_t clk_id, struct timespec* tp) {
  // TODO(gabylb) - zos: the following implements only CLOCK_REALTIME, but
  // should be sufficient as it's used only to get elapsed time (begin/end
  // calls to TicksNow()). Remove when clock_gettime() is available on z/OS.
  unsigned long long value;
  __stckf(&value);
  tp->tv_sec = (value / 4096000000UL) - 2208988800UL;
  tp->tv_nsec = (value % 4096000000UL) * 1000 / 4096;
  return 0;
}
#endif

namespace {

bool g_initialized;

#if defined(OS_WIN)
LARGE_INTEGER g_frequency;
LARGE_INTEGER g_start;
#elif defined(OS_MACOSX)
mach_timebase_info_data_t g_timebase;
uint64_t g_start;
#elif defined(OS_POSIX)
uint64_t g_start;
#else
#error Port.
#endif

#if !defined(OS_MACOSX)
constexpr uint64_t kNano = 1'000'000'000;
#endif

void Init() {
  DCHECK(!g_initialized);

#if defined(OS_WIN)
  QueryPerformanceFrequency(&g_frequency);
  QueryPerformanceCounter(&g_start);
#elif defined(OS_MACOSX)
  mach_timebase_info(&g_timebase);
  g_start = (mach_absolute_time() * g_timebase.numer) / g_timebase.denom;
#elif defined(OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  g_start = static_cast<uint64_t>(ts.tv_sec) * kNano +
            static_cast<uint64_t>(ts.tv_nsec);
#else
#error Port.
#endif

  g_initialized = true;
}

}  // namespace

Ticks TicksNow() {
  static bool initialized = []() {
    Init();
    return true;
  }();
  DCHECK(initialized);

  Ticks now;

#if defined(OS_WIN)
  LARGE_INTEGER t;
  QueryPerformanceCounter(&t);
  now = ((t.QuadPart - g_start.QuadPart) * kNano) / g_frequency.QuadPart;
#elif defined(OS_MACOSX)
  now =
      ((mach_absolute_time() * g_timebase.numer) / g_timebase.denom) - g_start;
#elif defined(OS_POSIX)
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  now = (static_cast<uint64_t>(ts.tv_sec) * kNano +
         static_cast<uint64_t>(ts.tv_nsec)) -
        g_start;
#else
#error Port.
#endif

  return now;
}

TickDelta TicksDelta(Ticks new_ticks, Ticks old_ticks) {
  DCHECK(new_ticks >= old_ticks);
  return TickDelta(new_ticks - old_ticks);
}
