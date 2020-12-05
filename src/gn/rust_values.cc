// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_tool.h"
#include "gn/rust_values.h"
#include "gn/target.h"

RustValues::RustValues() : crate_type_(RustValues::CRATE_AUTO) {}

RustValues::~RustValues() = default;

RustValues::CrateType RustValues::InferredCrateType(const Target* target) const {
  if (crate_type_ != CRATE_AUTO) {
      return crate_type_;
  }

  switch (target->output_type()) {
    case Target::EXECUTABLE:
      return CRATE_BIN;
    case Target::SHARED_LIBRARY:
      return CRATE_DYLIB;
    case Target::STATIC_LIBRARY:
      return CRATE_STATICLIB;
    case Target::RUST_LIBRARY:
      return CRATE_RLIB;
    case Target::RUST_PROC_MACRO:
      return CRATE_PROC_MACRO;
    default:
      return CRATE_AUTO;
  }
}