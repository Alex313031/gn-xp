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

const char* Toolchain::kToolCc = "cc";
const char* Toolchain::kToolCxx = "cxx";
const char* Toolchain::kToolObjC = "objc";
const char* Toolchain::kToolObjCxx = "objcxx";
const char* Toolchain::kToolRc = "rc";
const char* Toolchain::kToolAsm = "asm";
const char* Toolchain::kToolAlink = "alink";
const char* Toolchain::kToolSolink = "solink";
const char* Toolchain::kToolSolinkModule = "solink_module";
const char* Toolchain::kToolLink = "link";
const char* Toolchain::kToolStamp = "stamp";
const char* Toolchain::kToolCopy = "copy";
const char* Toolchain::kToolCopyBundleData = "copy_bundle_data";
const char* Toolchain::kToolCompileXCAssets = "compile_xcassets";
const char* Toolchain::kToolAction = "action";

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

// static
Tool::Tool::ToolType Toolchain::ToolNameToType(const base::StringPiece& str) {
  if (str == kToolCc)
    return Tool::TYPE_CC;
  if (str == kToolCxx)
    return Tool::TYPE_CXX;
  if (str == kToolObjC)
    return Tool::TYPE_OBJC;
  if (str == kToolObjCxx)
    return Tool::TYPE_OBJCXX;
  if (str == kToolRc)
    return Tool::TYPE_RC;
  if (str == kToolAsm)
    return Tool::TYPE_ASM;
  if (str == kToolAlink)
    return Tool::TYPE_ALINK;
  if (str == kToolSolink)
    return Tool::TYPE_SOLINK;
  if (str == kToolSolinkModule)
    return Tool::TYPE_SOLINK_MODULE;
  if (str == kToolLink)
    return Tool::TYPE_LINK;
  if (str == kToolStamp)
    return Tool::TYPE_STAMP;
  if (str == kToolCopy)
    return Tool::TYPE_COPY;
  if (str == kToolCopyBundleData)
    return Tool::TYPE_COPY_BUNDLE_DATA;
  if (str == kToolCompileXCAssets)
    return Tool::TYPE_COMPILE_XCASSETS;
  if (str == kToolAction)
    return Tool::TYPE_ACTION;
  return Tool::TYPE_NONE;
}

// static
std::string Toolchain::ToolTypeToName(Tool::ToolType type) {
  switch (type) {
    case Tool::TYPE_CC:
      return kToolCc;
    case Tool::TYPE_CXX:
      return kToolCxx;
    case Tool::TYPE_OBJC:
      return kToolObjC;
    case Tool::TYPE_OBJCXX:
      return kToolObjCxx;
    case Tool::TYPE_RC:
      return kToolRc;
    case Tool::TYPE_ASM:
      return kToolAsm;
    case Tool::TYPE_ALINK:
      return kToolAlink;
    case Tool::TYPE_SOLINK:
      return kToolSolink;
    case Tool::TYPE_SOLINK_MODULE:
      return kToolSolinkModule;
    case Tool::TYPE_LINK:
      return kToolLink;
    case Tool::TYPE_STAMP:
      return kToolStamp;
    case Tool::TYPE_COPY:
      return kToolCopy;
    case Tool::TYPE_COPY_BUNDLE_DATA:
      return kToolCopyBundleData;
    case Tool::TYPE_COMPILE_XCASSETS:
      return kToolCompileXCAssets;
    case Tool::TYPE_ACTION:
      return kToolAction;
    default:
      NOTREACHED();
      return std::string();
  }
}

Tool* Toolchain::GetTool(Tool::ToolType type) {
  DCHECK(type != Tool::TYPE_NONE);
  return tools_[static_cast<size_t>(type)].get();
}

const Tool* Toolchain::GetTool(Tool::ToolType type) const {
  DCHECK(type != Tool::TYPE_NONE);
  return tools_[static_cast<size_t>(type)].get();
}

void Toolchain::SetTool(Tool::ToolType type, std::unique_ptr<Tool> t) {
  DCHECK(type != Tool::TYPE_NONE);
  DCHECK(!tools_[type].get());
  t->SetComplete();
  tools_[type] = std::move(t);
}

void Toolchain::ToolchainSetupComplete() {
  // Collect required bits from all tools.
  for (const auto& tool : tools_) {
    if (tool)
      substitution_bits_.MergeFrom(tool->substitution_bits());
  }

  setup_complete_ = true;
}

// static
Tool::ToolType Toolchain::GetToolTypeForSourceType(SourceFileType type) {
  switch (type) {
    case SOURCE_C:
      return Tool::TYPE_CC;
    case SOURCE_CPP:
      return Tool::TYPE_CXX;
    case SOURCE_M:
      return Tool::TYPE_OBJC;
    case SOURCE_MM:
      return Tool::TYPE_OBJCXX;
    case SOURCE_ASM:
    case SOURCE_S:
      return Tool::TYPE_ASM;
    case SOURCE_RC:
      return Tool::TYPE_RC;
    case SOURCE_UNKNOWN:
    case SOURCE_H:
    case SOURCE_O:
    case SOURCE_DEF:
      return Tool::TYPE_NONE;
    default:
      NOTREACHED();
      return Tool::TYPE_NONE;
  }
}

const Tool* Toolchain::GetToolForSourceType(SourceFileType type) {
  return tools_[GetToolTypeForSourceType(type)].get();
}

// static
Tool::ToolType Toolchain::GetToolTypeForTargetFinalOutput(
    const Target* target) {
  // The contents of this list might be suprising (i.e. stamp tool for copy
  // rules). See the header for why.
  // TODO(crbug.com/gn/39): Don't emit stamp files for single-output targets.
  switch (target->output_type()) {
    case Target::GROUP:
      return Tool::TYPE_STAMP;
    case Target::EXECUTABLE:
      return Tool::Tool::TYPE_LINK;
    case Target::SHARED_LIBRARY:
      return Tool::Tool::TYPE_SOLINK;
    case Target::LOADABLE_MODULE:
      return Tool::Tool::TYPE_SOLINK_MODULE;
    case Target::STATIC_LIBRARY:
      return Tool::Tool::TYPE_ALINK;
    case Target::SOURCE_SET:
      return Tool::TYPE_STAMP;
    case Target::ACTION:
    case Target::ACTION_FOREACH:
    case Target::BUNDLE_DATA:
    case Target::CREATE_BUNDLE:
    case Target::COPY_FILES:
    case Target::GENERATED_FILE:
      return Tool::TYPE_STAMP;
    default:
      NOTREACHED();
      return Tool::Tool::TYPE_NONE;
  }
}

const Tool* Toolchain::GetToolForTargetFinalOutput(const Target* target) const {
  return tools_[GetToolTypeForTargetFinalOutput(target)].get();
}
