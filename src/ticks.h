// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TICKS_H_
#define TICKS_H_

#include <stdint.h>

void TicksInit();

using Ticks = uint64_t;
using TickDelta = uint64_t;

Ticks TicksNow();

TickDelta TicksDelta(Ticks old_ticks, Ticks new_ticks);

double TickDeltaInSeconds(TickDelta ticks);
double TickDeltaInMilliseconds(TickDelta ticks);
double TickDeltaInMicroseconds(TickDelta ticks);
double TickDeltaInNanoseconds(TickDelta ticks);

#endif  // TICKS_H_
