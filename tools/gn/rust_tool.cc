// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_tool.h"
#include "tools/gn/target.h"

const char* RustTool::kRsToolRust = "rust";
const char* RustTool::kRsToolRustAlink = "rust_alink";
const char* RustTool::kRsToolRustSolink = "rust_solink";
const char* RustTool::kRsToolRustSolinkModule = "rust_solink_module";
const char* RustTool::kRsToolRustProcMacro = "rust_proc_macro";

RustTool::RustTool(Tool::ToolType t) : Tool(t) {}

RustTool::~RustTool() = default;

RustTool* RustTool::AsRust() {
  return this;
}
const RustTool* RustTool::AsRust() const {
  return this;
}
void RustTool::SetComplete() {
  SetToolComplete();
}

bool RustTool::ShouldWriteToolRule() const {
  return type_ != TYPE_ACTION;
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
    if (!IsValidRustOutputsSubstitution(cur_type)) {
      *err = Err(*value, "Pattern not valid here.",
                 "You used the pattern " +
                     std::string(kSubstitutionNames[cur_type]) +
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

bool RustTool::ValidateSubstitution(SubstitutionType sub_type) const {
  switch (type_) {
    case TYPE_RS:
    case TYPE_RS_ALINK:
    case TYPE_RS_SOLINK:
    case TYPE_RS_SOLINK_MODULE:
    case TYPE_RS_PROC_MACRO:
      return IsValidRustSubstitution(sub_type);
    default:
      NOTREACHED();
      return false;
  }
}
