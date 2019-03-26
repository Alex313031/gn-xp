// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/tool.h"
#include "tools/gn/target.h"
#include "tools/gn/c_tool.h"
#include "tools/gn/general_tool.h"
#include "tools/gn/target.h"

Tool::Tool(Tool::ToolType t)
    : defined_from_(nullptr), restat_(false), complete_(false), type_(t) {}

Tool::~Tool() = default;

void Tool::SetToolComplete() {
  DCHECK(!complete_);
  complete_ = true;

  command_.FillRequiredTypes(&substitution_bits_);
  depfile_.FillRequiredTypes(&substitution_bits_);
  description_.FillRequiredTypes(&substitution_bits_);
  outputs_.FillRequiredTypes(&substitution_bits_);
  rspfile_.FillRequiredTypes(&substitution_bits_);
  rspfile_content_.FillRequiredTypes(&substitution_bits_);
}

GeneralTool* Tool::AsGeneral() {
  return nullptr;
}
const GeneralTool* Tool::AsGeneral() const {
  return nullptr;
}

CTool* Tool::AsC() {
  return nullptr;
}
const CTool* Tool::AsC() const {
  return nullptr;
}


bool Tool::IsPatternInOutputList(const SubstitutionList& output_list,
                           const SubstitutionPattern& pattern) const {
  for (const auto& cur : output_list.list()) {
    if (pattern.ranges().size() == cur.ranges().size() &&
        std::equal(pattern.ranges().begin(), pattern.ranges().end(),
                   cur.ranges().begin()))
      return true;
  }
  return false;
}

bool Tool::ValidateSubstitutionList(const std::vector<SubstitutionType>& list,
                                    const Value* origin,
                                    Err* err) const {
  for (const auto& cur_type : list) {
    if (!ValidateSubstitution(cur_type)) {
      *err = Err(*origin, "Pattern not valid here.",
                 "You used the pattern " +
                     std::string(kSubstitutionNames[cur_type]) +
                     " which is not valid\nfor this variable.");
      return false;
    }
  }
  return true;
}

bool Tool::ReadBool(Scope* scope,
                    const char* var,
                    bool* field,
                    Err* err) {
  DCHECK(!complete_);
  const Value* v = scope->GetValue(var, true);
  if (!v)
    return true;  // Not present is fine.
  if (!v->VerifyTypeIs(Value::BOOLEAN, err))
    return false;
  *field = v->boolean_value();
  return true;
}

bool Tool::ReadString(Scope* scope,
                      const char* var,
                      std::string* field,
                      Err* err) {
  DCHECK(!complete_);
  const Value* v = scope->GetValue(var, true);
  if (!v)
    return true;  // Not present is fine.
  if (!v->VerifyTypeIs(Value::STRING, err))
    return false;
  *field = v->string_value();
  return true;
}

bool Tool::ReadPattern(Scope* scope,
                       const char* var,
                       SubstitutionPattern* field,
                       Err* err) {
  DCHECK(!complete_);
  const Value* value = scope->GetValue(var, true);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::STRING, err))
    return false;

  SubstitutionPattern pattern;
  if (!pattern.Parse(*value, err))
    return false;
  if (!ValidateSubstitutionList(pattern.required_types(), value, err))
    return false;

  *field = std::move(pattern);
  return true;
}

bool Tool::ReadPatternList(Scope* scope,
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
  if (!ValidateSubstitutionList(list.required_types(), value, err))
    return false;

  *field = std::move(list);
  return true;
}

bool Tool::ReadLabel(Scope* scope,
                     const char* var,
                     const Label& current_toolchain,
                     LabelPtrPair<Pool>* field,
                     Err* err) {
  DCHECK(!complete_);
  const Value* v = scope->GetValue(var, true);
  if (!v)
    return true;  // Not present is fine.

  Label label =
      Label::Resolve(scope->GetSourceDir(), current_toolchain, *v, err);
  if (err->has_error())
    return false;

  LabelPtrPair<Pool> pair(label);
  pair.origin = defined_from();

  *field = std::move(pair);
  return true;
}

bool Tool::ReadOutputExtension(Scope* scope, Err* err) {
  DCHECK(!complete_);
  const Value* value = scope->GetValue("default_output_extension", true);
  if (!value)
    return true;  // Not present is fine.
  if (!value->VerifyTypeIs(Value::STRING, err))
    return false;

  if (value->string_value().empty())
    return true;  // Accept empty string.

  if (value->string_value()[0] != '.') {
    *err = Err(*value, "default_output_extension must begin with a '.'");
    return false;
  }

  set_default_output_extension(value->string_value());
  return true;
}

bool Tool::InitTool(Scope* scope, Toolchain* toolchain, Err* err) {
  if (!ReadPattern(scope, "command", &command_, err) ||
      !ReadOutputExtension(scope, err) ||
      !ReadPattern(scope, "depfile", &depfile_, err) ||
      !ReadPattern(scope, "description", &description_, err) ||
      !ReadPatternList(scope, "runtime_outputs", &runtime_outputs_, err) ||
      !ReadString(scope, "output_prefix", &output_prefix_, err) ||
      !ReadPattern(scope, "default_output_dir", &default_output_dir_, err) ||
      !ReadBool(scope, "restat", &restat_, err) ||
      !ReadPattern(scope, "rspfile", &rspfile_, err) ||
      !ReadPattern(scope, "rspfile_content", &rspfile_content_, err) ||
      !ReadLabel(scope, "pool", toolchain->label(), &pool_, err)) {
    return false;
  }
  return true;
}

// static
std::unique_ptr<Tool> Tool::CreateTool(Tool::ToolType type) {
  switch (type) {
    case TYPE_CC:
    case TYPE_CXX:
    case TYPE_OBJC:
    case TYPE_OBJCXX:
    case TYPE_RC:
    case TYPE_ASM:
    case TYPE_ALINK:
    case TYPE_SOLINK:
    case TYPE_SOLINK_MODULE:
    case TYPE_LINK:
      return std::make_unique<CTool>(type);
    case TYPE_STAMP:
    case TYPE_COPY:
    case TYPE_COPY_BUNDLE_DATA:
    case TYPE_COMPILE_XCASSETS:
    case TYPE_ACTION:
      return std::make_unique<GeneralTool>(type);
    default:
      NOTREACHED();
      return nullptr;
  }
}

std::unique_ptr<Tool> Tool::CreateTool(Tool::ToolType type,
                                       Scope* scope,
                                       Toolchain* toolchain,
                                       Err* err) {
  switch (type) {
    case TYPE_CC:
    case TYPE_CXX:
    case TYPE_OBJC:
    case TYPE_OBJCXX:
    case TYPE_RC:
    case TYPE_ASM:
    case TYPE_ALINK:
    case TYPE_SOLINK:
    case TYPE_SOLINK_MODULE:
    case TYPE_LINK: {
      std::unique_ptr<Tool> tool = std::make_unique<CTool>(type);
      if (tool->AsC()->InitTool(scope, toolchain, err))
        return tool;
      return nullptr;
    }
    case TYPE_STAMP:
    case TYPE_COPY:
    case TYPE_COPY_BUNDLE_DATA:
    case TYPE_COMPILE_XCASSETS:
    case TYPE_ACTION: {
      std::unique_ptr<Tool> tool = std::make_unique<GeneralTool>(type);
      tool->AsGeneral()->InitTool(scope, toolchain, err);
      return tool;
    }
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
Tool::ToolType Tool::ToolNameToType(const base::StringPiece& str) {
  if (str == CTool::kCToolCc)
    return TYPE_CC;
  if (str == CTool::kCToolCxx)
    return TYPE_CXX;
  if (str == CTool::kCToolObjC)
    return TYPE_OBJC;
  if (str == CTool::kCToolObjCxx)
    return TYPE_OBJCXX;
  if (str == CTool::kCToolRc)
    return TYPE_RC;
  if (str == CTool::kCToolAsm)
    return TYPE_ASM;
  if (str == CTool::kCToolAlink)
    return TYPE_ALINK;
  if (str == CTool::kCToolSolink)
    return TYPE_SOLINK;
  if (str == CTool::kCToolSolinkModule)
    return TYPE_SOLINK_MODULE;
  if (str == CTool::kCToolLink)
    return TYPE_LINK;
  if (str == GeneralTool::kGeneralToolStamp)
    return TYPE_STAMP;
  if (str == GeneralTool::kGeneralToolCopy)
    return TYPE_COPY;
  if (str == GeneralTool::kGeneralToolCopyBundleData)
    return TYPE_COPY_BUNDLE_DATA;
  if (str == GeneralTool::kGeneralToolCompileXCAssets)
    return TYPE_COMPILE_XCASSETS;
  if (str == GeneralTool::kGeneralToolAction)
    return TYPE_ACTION;
  return TYPE_NONE;
}

// static
std::string Tool::ToolTypeToName(Tool::ToolType type) {
  switch (type) {
    case TYPE_CC:
      return CTool::kCToolCc;
    case TYPE_CXX:
      return CTool::kCToolCxx;
    case TYPE_OBJC:
      return CTool::kCToolObjC;
    case TYPE_OBJCXX:
      return CTool::kCToolObjCxx;
    case TYPE_RC:
      return CTool::kCToolRc;
    case TYPE_ASM:
      return CTool::kCToolAsm;
    case TYPE_ALINK:
      return CTool::kCToolAlink;
    case TYPE_SOLINK:
      return CTool::kCToolSolink;
    case TYPE_SOLINK_MODULE:
      return CTool::kCToolSolinkModule;
    case TYPE_LINK:
      return CTool::kCToolLink;
    case TYPE_STAMP:
      return GeneralTool::kGeneralToolStamp;
    case TYPE_COPY:
      return GeneralTool::kGeneralToolCopy;
    case TYPE_COPY_BUNDLE_DATA:
      return GeneralTool::kGeneralToolCopyBundleData;
    case TYPE_COMPILE_XCASSETS:
      return GeneralTool::kGeneralToolCompileXCAssets;
    case TYPE_ACTION:
      return GeneralTool::kGeneralToolAction;
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
