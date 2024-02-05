// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UTIL_SYS_INFO_H_
#define UTIL_SYS_INFO_H_

#include <string>

std::string OperatingSystemArchitecture();
int NumberOfProcessors();
bool IsLongPathSupported();

#endif  // UTIL_SYS_INFO_H_
