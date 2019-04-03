// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_tool.h"
#include "tools/gn/rust_substitution_type.h"
#include "tools/gn/target.h"

const char* RustTool::kRsToolRust = "rust";
const char* RustTool::kRsToolRustAlink = "rust_alink";
const char* RustTool::kRsToolRustSolink = "rust_solink";
const char* RustTool::kRsToolRustSolinkModule = "rust_solink_module";
const char* RustTool::kRsToolRustProcMacro = "rust_proc_macro";

RustTool::RustTool(const char* n) : Tool(n) {
  CHECK(ValidateName(n));
}

RustTool::~RustTool() = default;

RustTool* RustTool::AsRust() {
  return this;
}
const RustTool* RustTool::AsRust() const {
  return this;
}

bool RustTool::ValidateName(const char* name) const {
  return name_ == kRsToolRust || name_ == kRsToolRustAlink ||
         name_ == kRsToolRustSolink || name_ == kRsToolRustSolinkModule ||
         name_ == kRsToolRustProcMacro;
}

void RustTool::SetComplete() {
  SetToolComplete();
}

bool RustTool::InitTool(Scope* scope, Toolchain* toolchain, Err* err) {
  // Initialize default vars.
  return Tool::InitTool(scope, toolchain, err);
}

bool RustTool::ValidateSubstitution(const Substitution* sub_type) const {
  if (name_ == kRsToolRust || name_ == kRsToolRustAlink ||
      name_ == kRsToolRustSolink || name_ == kRsToolRustSolinkModule ||
      name_ == kRsToolRustProcMacro)
    return IsValidRustSubstitution(sub_type);
  NOTREACHED();
  return false;
}
