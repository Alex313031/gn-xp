// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/general_tool.h"
#include "tools/gn/target.h"

const char* GeneralTool::kGeneralToolStamp = "stamp";
const char* GeneralTool::kGeneralToolCopy = "copy";
const char* GeneralTool::kGeneralToolCopyBundleData = "copy_bundle_data";
const char* GeneralTool::kGeneralToolCompileXCAssets = "compile_xcassets";
const char* GeneralTool::kGeneralToolAction = "action";

GeneralTool::GeneralTool(Tool::ToolType t) : Tool(t) {}

GeneralTool::~GeneralTool() = default;

GeneralTool* GeneralTool::AsGeneral() {
  return this;
}
const GeneralTool* GeneralTool::AsGeneral() const {
  return this;
}
void GeneralTool::SetComplete() {
  SetToolComplete();
}

bool GeneralTool::ShouldWriteToolRule() const {
  return type_ != TYPE_ACTION;
}

bool GeneralTool::InitTool(Scope* scope, Toolchain* toolchain, Err* err) {
  // Initialize default vars.
  return Tool::InitTool(scope, toolchain, err);
}

bool GeneralTool::ValidateSubstitution(SubstitutionType sub_type) const {
  switch (type_) {
    case TYPE_STAMP:
    case TYPE_ACTION:
      return IsValidToolSubstitution(sub_type);
    case TYPE_COPY:
    case TYPE_COPY_BUNDLE_DATA:
      return IsValidCopySubstitution(sub_type);
    case TYPE_COMPILE_XCASSETS:
      return IsValidCompileXCassetsSubstitution(sub_type);
    default:
      NOTREACHED();
      return false;
  }
}
