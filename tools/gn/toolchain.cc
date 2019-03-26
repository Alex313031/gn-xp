// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/toolchain.h"

#include <stddef.h>
#include <string.h>
#include <utility>

#include "base/logging.h"
#include "tools/gn/target.h"
#include "tools/gn/value.h"

Toolchain::Toolchain(const Settings* settings,
                     const Label& label,
                     const std::set<SourceFile>& build_dependency_files)
    : Item(settings, label, build_dependency_files) {}

Toolchain::~Toolchain() = default;

Toolchain* Toolchain::AsToolchain() {
  return this;
}

const Toolchain* Toolchain::AsToolchain() const {
  return this;
}

Tool* Toolchain::GetTool(Tool::ToolType type) {
  DCHECK(type != Tool::TYPE_NONE);
  auto t = tools_.find(type);
  if (t != tools_.end()) {
    return t->second.get();
  }
  return nullptr;
}

const Tool* Toolchain::GetTool(Tool::ToolType type) const {
  DCHECK(type != Tool::TYPE_NONE);
  const auto t = tools_.find(type);
  if (t != tools_.end()) {
    return t->second.get();
  }
  return nullptr;
}

GeneralTool* Toolchain::GetToolAsGeneral(Tool::ToolType type) {
  if (Tool* tool = GetTool(type)) {
    return tool->AsGeneral();
  }
  return nullptr;
}

const GeneralTool* Toolchain::GetToolAsGeneral(Tool::ToolType type) const {
  if (const Tool* tool = GetTool(type)) {
    return tool->AsGeneral();
  }
  return nullptr;
}

CTool* Toolchain::GetToolAsC(Tool::ToolType type) {
  if (Tool* tool = GetTool(type)) {
    return tool->AsC();
  }
  return nullptr;
}

const CTool* Toolchain::GetToolAsC(Tool::ToolType type) const {
  if (const Tool* tool = GetTool(type)) {
    return tool->AsC();
  }
  return nullptr;
}

void Toolchain::SetTool(std::unique_ptr<Tool> t) {
  DCHECK(t);
  DCHECK(t->type() != Tool::TYPE_NONE);
  DCHECK(tools_.find(t->type()) == tools_.end());
  t->SetComplete();
  tools_.insert(std::make_pair<Tool::ToolType, std::unique_ptr<Tool>>(
      t->type(), std::move(t)));
}

void Toolchain::ToolchainSetupComplete() {
  // Collect required bits from all tools.
  for (const auto& tool : tools_) {
    substitution_bits_.MergeFrom(tool.second->substitution_bits());
  }
  setup_complete_ = true;
}

const Tool* Toolchain::GetToolForSourceType(SourceFileType type) const {
  return GetTool(Tool::GetToolTypeForSourceType(type));
}

const CTool* Toolchain::GetToolForSourceTypeAsC(SourceFileType type) const {
  return GetToolAsC(Tool::GetToolTypeForSourceType(type));
}

const GeneralTool* Toolchain::GetToolForSourceTypeAsGeneral(
    SourceFileType type) const {
  return GetToolAsGeneral(Tool::GetToolTypeForSourceType(type));
}

const Tool* Toolchain::GetToolForTargetFinalOutput(const Target* target) const {
  return GetTool(Tool::GetToolTypeForTargetFinalOutput(target));
}

const CTool* Toolchain::GetToolForTargetFinalOutputAsC(
    const Target* target) const {
  return GetToolAsC(Tool::GetToolTypeForTargetFinalOutput(target));
}

const GeneralTool* Toolchain::GetToolForTargetFinalOutputAsGeneral(
    const Target* target) const {
  return GetToolAsGeneral(Tool::GetToolTypeForTargetFinalOutput(target));
}
