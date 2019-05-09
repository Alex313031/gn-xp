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

  enum CrateType {
    CRATE_AUTO = 0,
    CRATE_DYLIB,
    CRATE_CDYLIB,
    CRATE_PROC_MACRO,
  };

  // Name of this crate.
  std::string& crate_name() { return crate_name_; }
  const std::string& crate_name() const { return crate_name_; }

  // Main source file for this crate.
  const SourceFile* crate_root() const { return crate_root_; }
  void set_crate_root(SourceFile* s) { crate_root_ = s; }

  // Crate type for compilation.
  CrateType crate_type() { return crate_type_; }
  const CrateType crate_type() const { return crate_type_; }
  void set_crate_type(CrateType s) { crate_type_ = s; }

 private:
  std::string crate_name_;
  SourceFile* crate_root_;
  CrateType crate_type_;

  DISALLOW_COPY_AND_ASSIGN(RustValues);
};

#endif  // TOOLS_GN_RUST_TARGET_VALUES_H_
