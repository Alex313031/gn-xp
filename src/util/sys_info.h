// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SYS_INFO_H_
#define UTIL_SYS_INFO_H_

#include <string>

bool IsLongPathsSupportEnabled();
std::string OperatingSystemArchitecture();
int NumberOfProcessors();
bool IsLegacyWindows();

// Human-readable OS version, e.g. "Windows XP (5.1.2600)". Returns an empty
// string when the version is unavailable or on non-Windows platforms.
std::string OperatingSystemVersion();

#endif  // UTIL_SYS_INFO_H_
