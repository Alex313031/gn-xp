// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SWIFT_TARGET_VALUES_H_
#define TOOLS_GN_SWIFT_TARGET_VALUES_H_

#include <string>

#include "gn/source_file.h"

class SwiftValues {
 public:
  SwiftValues();
  ~SwiftValues();

  SwiftValues(const SwiftValues&) = delete;
  SwiftValues& operator=(const SwiftValues&) = delete;

  // Path of the bridging header.
  SourceFile& bridge_header() { return bridge_header_; }
  const SourceFile& bridge_header() const { return bridge_header_; }

  // Name of the module.
  std::string& module_name() { return module_name_; }
  const std::string module_name() const { return module_name_; }

 private:
  SourceFile bridge_header_;
  std::string module_name_;
};

#endif  // TOOLS_GN_SWIFT_TARGET_VALUES_H_
