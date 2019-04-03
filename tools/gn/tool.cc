// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/tool.h"
#include "tools/gn/target.h"

const char* Tool::kToolNone = "";
const char* Tool::kToolCc = "cc";
const char* Tool::kToolCxx = "cxx";
const char* Tool::kToolObjC = "objc";
const char* Tool::kToolObjCxx = "objcxx";
const char* Tool::kToolRc = "rc";
const char* Tool::kToolAsm = "asm";
const char* Tool::kToolAlink = "alink";
const char* Tool::kToolSolink = "solink";
const char* Tool::kToolSolinkModule = "solink_module";
const char* Tool::kToolLink = "link";
const char* Tool::kToolStamp = "stamp";
const char* Tool::kToolCopy = "copy";
const char* Tool::kToolCopyBundleData = "copy_bundle_data";
const char* Tool::kToolCompileXCAssets = "compile_xcassets";
const char* Tool::kToolAction = "action";

Tool::Tool(const char* name)
    : defined_from_(nullptr),
      depsformat_(DEPS_GCC),
      precompiled_header_type_(PCH_NONE),
      restat_(false),
      complete_(false),
      name_(name) {
  CHECK(ValidateName(name));
}

Tool::~Tool() = default;

bool Tool::ValidateName(const char* name) {
  return name == kToolCc || name == kToolCxx || name == kToolObjC ||
         name == kToolObjCxx || name == kToolRc || name == kToolAsm ||
         name == kToolAlink || name == kToolSolink ||
         name == kToolSolinkModule || name == kToolLink || name == kToolStamp ||
         name == kToolCopy || name == kToolCopyBundleData ||
         name == kToolCompileXCAssets || name == kToolAction;
}

void Tool::SetComplete() {
  DCHECK(!complete_);
  complete_ = true;

  command_.FillRequiredTypes(&substitution_bits_);
  depfile_.FillRequiredTypes(&substitution_bits_);
  description_.FillRequiredTypes(&substitution_bits_);
  outputs_.FillRequiredTypes(&substitution_bits_);
  link_output_.FillRequiredTypes(&substitution_bits_);
  depend_output_.FillRequiredTypes(&substitution_bits_);
  rspfile_.FillRequiredTypes(&substitution_bits_);
  rspfile_content_.FillRequiredTypes(&substitution_bits_);
}

// static
std::unique_ptr<Tool> Tool::CreateTool(const std::string& name) {
  if (name == Tool::kToolCc)
    return std::make_unique<Tool>(Tool::kToolCc);
  else if (name == Tool::kToolCxx)
    return std::make_unique<Tool>(Tool::kToolCxx);
  else if (name == Tool::kToolObjC)
    return std::make_unique<Tool>(Tool::kToolObjC);
  else if (name == Tool::kToolObjCxx)
    return std::make_unique<Tool>(Tool::kToolObjCxx);
  else if (name == Tool::kToolRc)
    return std::make_unique<Tool>(Tool::kToolRc);
  else if (name == Tool::kToolAsm)
    return std::make_unique<Tool>(Tool::kToolAsm);
  else if (name == Tool::kToolAlink)
    return std::make_unique<Tool>(Tool::kToolAlink);
  else if (name == Tool::kToolSolink)
    return std::make_unique<Tool>(Tool::kToolSolink);
  else if (name == Tool::kToolSolinkModule)
    return std::make_unique<Tool>(Tool::kToolSolinkModule);
  else if (name == Tool::kToolLink)
    return std::make_unique<Tool>(Tool::kToolLink);

  else if (name == Tool::kToolAction)
    return std::make_unique<Tool>(Tool::kToolAction);
  else if (name == Tool::kToolStamp)
    return std::make_unique<Tool>(Tool::kToolStamp);
  else if (name == Tool::kToolCopy)
    return std::make_unique<Tool>(Tool::kToolCopy);
  else if (name == Tool::kToolCopyBundleData)
    return std::make_unique<Tool>(Tool::kToolCopyBundleData);
  else if (name == Tool::kToolCompileXCAssets)
    return std::make_unique<Tool>(Tool::kToolCompileXCAssets);

  return nullptr;
}

// static
const char* Tool::GetToolTypeForSourceType(SourceFileType type) {
  switch (type) {
    case SOURCE_C:
      return kToolCc;
    case SOURCE_CPP:
      return kToolCxx;
    case SOURCE_M:
      return kToolObjC;
    case SOURCE_MM:
      return kToolObjCxx;
    case SOURCE_ASM:
    case SOURCE_S:
      return kToolAsm;
    case SOURCE_RC:
      return kToolRc;
    case SOURCE_UNKNOWN:
    case SOURCE_H:
    case SOURCE_O:
    case SOURCE_DEF:
      return kToolNone;
    default:
      NOTREACHED();
      return kToolNone;
  }
}

// static
const char* Tool::GetToolTypeForTargetFinalOutput(const Target* target) {
  // The contents of this list might be suprising (i.e. stamp tool for copy
  // rules). See the header for why.
  // TODO(crbug.com/gn/39): Don't emit stamp files for single-output targets.
  switch (target->output_type()) {
    case Target::GROUP:
      return kToolStamp;
    case Target::EXECUTABLE:
      return Tool::kToolLink;
    case Target::SHARED_LIBRARY:
      return Tool::kToolSolink;
    case Target::LOADABLE_MODULE:
      return Tool::kToolSolinkModule;
    case Target::STATIC_LIBRARY:
      return Tool::kToolAlink;
    case Target::SOURCE_SET:
      return kToolStamp;
    case Target::ACTION:
    case Target::ACTION_FOREACH:
    case Target::BUNDLE_DATA:
    case Target::CREATE_BUNDLE:
    case Target::COPY_FILES:
    case Target::GENERATED_FILE:
      return kToolStamp;
    default:
      NOTREACHED();
      return kToolNone;
  }
}
