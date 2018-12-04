// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/write_data_target_generator.h"

#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/target.h"
#include "tools/gn/variables.h"

WriteDataTargetGenerator::WriteDataTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Target::OutputType type,
    Err* err)
    : TargetGenerator(target, scope, function_call, err),
      output_type_(type),
      data_defined_(false),
      data_keys_defined_(false) {}

WriteDataTargetGenerator::~WriteDataTargetGenerator() = default;

void WriteDataTargetGenerator::DoRun() {
  target_->set_output_type(output_type_);

  if (!FillWriteDataOutput())
    return;
  if (!FillOutputConversion())
    return;

  if (!FillData())
    return;
  if (!FillDataKeys())
    return;

  // One of data and data_keys should be defined.
  if (!data_defined_ && !data_keys_defined_) {
    *err_ = Err(function_call_, "Either data or data_keys should be set.",
                "write_data wants some sort of data to write.");
    return;
  }

  if (!FillRebase())
    return;
  if (!FillWalkKeys())
    return;
}

bool WriteDataTargetGenerator::FillData() {
  const Value* value = scope_->GetValue(variables::kWriteDataValue, true);
  if (!value)
    return true;
  target_->set_write_data(*value);
  data_defined_ = true;
  return true;
}

bool WriteDataTargetGenerator::DataDefined(const base::StringPiece& name) {
  if (data_defined_) {
    *err_ =
        Err(function_call_, name.as_string() + " won't be used.",
            "write_data is defined on this target, and so setting " +
                name.as_string() +
                " will have no effect as no metdata collection will occur.");
    return true;
  }
  return false;
}

bool WriteDataTargetGenerator::FillWriteDataOutput() {
  const Value* value = scope_->GetValue(variables::kWriteDataOutput, true);
  if (!value) {
    *err_ = Err(function_call_, "Missing write_data_output definition.",
                "This target requires this variable to be set.");
    return false;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  // Compute the file name and make sure it's in the output dir.
  SourceFile source_file = target_->output_dir().ResolveRelativeFile(
      *value, err_, GetBuildSettings()->root_path_utf8());
  if (err_->has_error())
    return false;
  if (!EnsureStringIsInOutputDir(GetBuildSettings()->build_dir(),
                                 source_file.value(), value->origin(), err_))
    return false;
  OutputFile output_file(GetBuildSettings(), source_file);
  target_->set_write_data_output(output_file);
  return true;
}

bool WriteDataTargetGenerator::FillOutputConversion() {
  const Value* value =
      scope_->GetValue(variables::kWriteOutputConversion, true);
  if (!value) {
    target_->set_write_output_conversion(Value(function_call_, "json"));
    return true;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  // Check that this is a valid output conversion string.
  if (value->string_value() == "" || value->string_value() == "list lines" ||
      value->string_value() == "string" || value->string_value() == "value" ||
      value->string_value() == "json" || value->string_value() == "scope") {
    target_->set_write_output_conversion(*value);
    return true;
  }
  return false;
}

bool WriteDataTargetGenerator::FillRebase() {
  const Value* value = scope_->GetValue(variables::kRebase, true);
  if (!value)
    return true;
  if (DataDefined(variables::kRebase))
    return false;
  if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
    return false;
  target_->set_write_rebase(value->boolean_value());
  return true;
}

bool WriteDataTargetGenerator::FillDataKeys() {
  const Value* value = scope_->GetValue(variables::kDataKeys, true);
  if (!value)
    return true;
  if (DataDefined(variables::kDataKeys))
    return false;
  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  for (const Value& v : value->list_value()) {
    // Keys must be strings.
    if (!v.VerifyTypeIs(Value::STRING, err_))
      return false;
    target_->write_data_keys().push_back(v.string_value());
  }

  data_keys_defined_ = true;
  return true;
}

bool WriteDataTargetGenerator::FillWalkKeys() {
  const Value* value = scope_->GetValue(variables::kWalkKeys, true);
  if (!value)
    return true;
  if (DataDefined(variables::kWalkKeys))
    return false;
  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;
  for (const Value& v : value->list_value()) {
    // Keys must be strings.
    if (!v.VerifyTypeIs(Value::STRING, err_))
      return false;
    target_->write_walk_keys().push_back(v.string_value());
  }
  return true;
}
