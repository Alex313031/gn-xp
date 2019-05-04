// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_TARGET_VALUES_H_
#define TOOLS_GN_RUST_TARGET_VALUES_H_

#include "tools/gn/source_file.h"

// Holds the values (outputs, args, script name, etc.) for either an action or
// an action_foreach target.
class RustValues {
 public:
  RustValues();
  ~RustValues();

  // Filename of the script to execute.
  const SourceFile* crate_root() const { return crate_root_; }
  void set_crate_root(SourceFile* s) { crate_root_ = s; }

  // Arguments to the script.
  std::string& crate_type() { return crate_type_; }
  const std::string& crate_type() const { return crate_type_; }

 private:
  SourceFile* crate_root_;
  std::string crate_type_;

  DISALLOW_COPY_AND_ASSIGN(RustValues);
};

#endif  // TOOLS_GN_RUST_TARGET_VALUES_H_
