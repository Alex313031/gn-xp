// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_tool.h"
#include "tools/gn/rust_substitution_type.h"
#include "tools/gn/target.h"

const char* RustTool::kRsToolRustc = "rustc";

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
  return name_ == kRsToolRustc;
}

void RustTool::SetComplete() {
  SetToolComplete();
}

bool RustTool::ReadOutputsPatternList(Scope* scope,
                                      const char* var,
                                      SubstitutionList* field,
                                      Err* err) {
  DCHECK(!complete_);
  const Value* value = scope->GetValue(var, true);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::LIST, err))
    return false;

  SubstitutionList list;
  if (!list.Parse(*value, err))
    return false;

  // Validate the right kinds of patterns are used.
  if (list.list().empty()) {
    *err = Err(defined_from(), "\"outputs\" must be specified for this tool.");
    return false;
  }

  for (const auto& cur_type : list.required_types()) {
    if (!IsValidRustSubstitution(cur_type)) {
      *err = Err(*value, "Pattern not valid here.",
                 "You used the pattern " +
                     std::string(cur_type->name) +
                     " which is not valid\nfor this variable.");
      return false;
    }
  }

  *field = std::move(list);
  return true;
}

bool RustTool::InitTool(Scope* scope, Toolchain* toolchain, Err* err) {
  // Initialize default vars.
  if (!Tool::InitTool(scope, toolchain, err)) {
    return false;
  }

  // All Rust tools should have outputs.
  if (!ReadOutputsPatternList(scope, "outputs", &outputs_, err)) {
    return false;
  }

  return true;
}

bool RustTool::ValidateSubstitution(const Substitution* sub_type) const {
  if (name_ == kRsToolRustc)
    return IsValidRustSubstitution(sub_type);
  NOTREACHED();
  return false;
}
