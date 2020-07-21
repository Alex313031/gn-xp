// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SWIFT_TARGET_VALUES_H_
#define TOOLS_GN_SWIFT_TARGET_VALUES_H_

#include <string>

#include "gn/output_file.h"
#include "gn/source_file.h"
#include "gn/unique_vector.h"

class Target;

class SwiftValues {
 public:
  SwiftValues();
  ~SwiftValues();

  SwiftValues(const SwiftValues&) = delete;
  SwiftValues& operator=(const SwiftValues&) = delete;

  // Called when the target is resolved.
  void OnTargetResolved(const Target* target);

  // Path of the bridging header.
  SourceFile& bridge_header() { return bridge_header_; }
  const SourceFile& bridge_header() const { return bridge_header_; }

  // Name of the module.
  std::string& module_name() { return module_name_; }
  const std::string module_name() const { return module_name_; }

  // Name of the generated swiftmodule file. Computed when the target
  // is resolved. Will be empty before.
  OutputFile& module_output_file() { return module_output_file_; }
  const OutputFile& module_output_file() const { return module_output_file_; }

  // Swift module search path used to build the current Swift module.
  const UniqueVector<const Target*>& modules() const { return modules_; }

  // Swift module search path exported to dependencies (recursively via
  // public_deps).
  const UniqueVector<const Target*>& public_modules() const {
    return public_modules_;
  }

 private:
  SourceFile bridge_header_;
  std::string module_name_;
  OutputFile module_output_file_;
  UniqueVector<const Target*> modules_;
  UniqueVector<const Target*> public_modules_;
};

#endif  // TOOLS_GN_SWIFT_TARGET_VALUES_H_
