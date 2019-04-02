// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/tool.h"
#include "tools/gn/target.h"

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

Tool::Tool()
    : defined_from_(nullptr),
      depsformat_(DEPS_GCC),
      precompiled_header_type_(PCH_NONE),
      restat_(false),
      complete_(false) {}

Tool::~Tool() = default;

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
Tool::ToolType Tool::ToolNameToType(const base::StringPiece& str) {
  if (str == Tool::kToolCc)
    return TYPE_CC;
  if (str == Tool::kToolCxx)
    return TYPE_CXX;
  if (str == Tool::kToolObjC)
    return TYPE_OBJC;
  if (str == Tool::kToolObjCxx)
    return TYPE_OBJCXX;
  if (str == Tool::kToolRc)
    return TYPE_RC;
  if (str == Tool::kToolAsm)
    return TYPE_ASM;
  if (str == Tool::kToolAlink)
    return TYPE_ALINK;
  if (str == Tool::kToolSolink)
    return TYPE_SOLINK;
  if (str == Tool::kToolSolinkModule)
    return TYPE_SOLINK_MODULE;
  if (str == Tool::kToolLink)
    return TYPE_LINK;
  if (str == Tool::kToolStamp)
    return TYPE_STAMP;
  if (str == Tool::kToolCopy)
    return TYPE_COPY;
  if (str == Tool::kToolCopyBundleData)
    return TYPE_COPY_BUNDLE_DATA;
  if (str == Tool::kToolCompileXCAssets)
    return TYPE_COMPILE_XCASSETS;
  if (str == Tool::kToolAction)
    return TYPE_ACTION;
  return TYPE_NONE;
}

// static
std::string Tool::ToolTypeToName(Tool::ToolType type) {
  switch (type) {
    case TYPE_CC:
      return Tool::kToolCc;
    case TYPE_CXX:
      return Tool::kToolCxx;
    case TYPE_OBJC:
      return Tool::kToolObjC;
    case TYPE_OBJCXX:
      return Tool::kToolObjCxx;
    case TYPE_RC:
      return Tool::kToolRc;
    case TYPE_ASM:
      return Tool::kToolAsm;
    case TYPE_ALINK:
      return Tool::kToolAlink;
    case TYPE_SOLINK:
      return Tool::kToolSolink;
    case TYPE_SOLINK_MODULE:
      return Tool::kToolSolinkModule;
    case TYPE_LINK:
      return Tool::kToolLink;
    case TYPE_STAMP:
      return Tool::kToolStamp;
    case TYPE_COPY:
      return Tool::kToolCopy;
    case TYPE_COPY_BUNDLE_DATA:
      return Tool::kToolCopyBundleData;
    case TYPE_COMPILE_XCASSETS:
      return Tool::kToolCompileXCAssets;
    case TYPE_ACTION:
      return Tool::kToolAction;
    default:
      NOTREACHED();
      return std::string();
  }
}

// static
Tool::ToolType Tool::GetToolTypeForSourceType(SourceFileType type) {
  switch (type) {
    case SOURCE_C:
      return TYPE_CC;
    case SOURCE_CPP:
      return TYPE_CXX;
    case SOURCE_M:
      return TYPE_OBJC;
    case SOURCE_MM:
      return TYPE_OBJCXX;
    case SOURCE_ASM:
    case SOURCE_S:
      return TYPE_ASM;
    case SOURCE_RC:
      return TYPE_RC;
    case SOURCE_UNKNOWN:
    case SOURCE_H:
    case SOURCE_O:
    case SOURCE_DEF:
      return TYPE_NONE;
    default:
      NOTREACHED();
      return TYPE_NONE;
  }
}

// static
Tool::ToolType Tool::GetToolTypeForTargetFinalOutput(const Target* target) {
  // The contents of this list might be suprising (i.e. stamp tool for copy
  // rules). See the header for why.
  // TODO(crbug.com/gn/39): Don't emit stamp files for single-output targets.
  switch (target->output_type()) {
    case Target::GROUP:
      return TYPE_STAMP;
    case Target::EXECUTABLE:
      return Tool::TYPE_LINK;
    case Target::SHARED_LIBRARY:
      return Tool::TYPE_SOLINK;
    case Target::LOADABLE_MODULE:
      return Tool::TYPE_SOLINK_MODULE;
    case Target::STATIC_LIBRARY:
      return Tool::TYPE_ALINK;
    case Target::SOURCE_SET:
      return TYPE_STAMP;
    case Target::ACTION:
    case Target::ACTION_FOREACH:
    case Target::BUNDLE_DATA:
    case Target::CREATE_BUNDLE:
    case Target::COPY_FILES:
    case Target::GENERATED_FILE:
      return TYPE_STAMP;
    default:
      NOTREACHED();
      return Tool::TYPE_NONE;
  }
}
