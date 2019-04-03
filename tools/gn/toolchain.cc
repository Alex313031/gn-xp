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
  return tools_[static_cast<size_t>(type)].get();
}

const Tool* Toolchain::GetTool(Tool::ToolType type) const {
  DCHECK(type != Tool::TYPE_NONE);
  return tools_[static_cast<size_t>(type)].get();
}

void Toolchain::SetTool(std::unique_ptr<Tool> t) {
  DCHECK(t->type() != Tool::TYPE_NONE);
  DCHECK(!tools_[t->type()].get());
  t->SetComplete();
  tools_[t->type()] = std::move(t);
}

void Toolchain::ToolchainSetupComplete() {
  // Collect required bits from all tools.
  for (const auto& tool : tools_) {
    if (tool)
      substitution_bits_.MergeFrom(tool->substitution_bits());
  }

  setup_complete_ = true;
}

const Tool* Toolchain::GetToolForSourceType(SourceFileType type) {
  return tools_[Tool::GetToolTypeForSourceType(type)].get();
}

const Tool* Toolchain::GetToolForTargetFinalOutput(const Target* target) const {
  return tools_[Tool::GetToolTypeForTargetFinalOutput(target)].get();
}
