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
    : TargetGenerator(target, scope, function_call, err), output_type_(type) {}

WriteDataTargetGenerator::~WriteDataTargetGenerator() = default;

void WriteDataTargetGenerator::DoRun() {
  target_->set_output_type(output_type_);

  if (!FillOutputs(false))
    return;
  if (target_->action_values().outputs().list().size() != 1) {
    *err_ = Err(
        function_call_, "write_data command must have exactly one output.",
        "You must specify exactly one value in the \"outputs\" array for the "
        "destination of the write\n(see \"gn help write_data\").");
    return;
  }

  if (!FillContents()) {
    *err_ = Err(function_call_, "Contents should be set.",
                "write_data wants some sort of data to write.");
    return;
  }

  if (!FillOutputConversion())
    return;
}

bool WriteDataTargetGenerator::FillContents() {
  const Value* value = scope_->GetValue(variables::kWriteValueContents, true);
  if (!value)
    return false;
  target_->set_contents(*value);
  return true;
}

bool WriteDataTargetGenerator::FillOutputConversion() {
  const Value* value =
      scope_->GetValue(variables::kWriteOutputConversion, true);
  if (!value) {
    target_->set_output_conversion(Value(function_call_, ""));
    return true;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  // Otherwise, check that the specified value is accceptable.
  if (value->string_value() == "" || value->string_value() == "list lines" ||
      value->string_value() == "string" || value->string_value() == "value" ||
      value->string_value() == "json" || value->string_value() == "scope") {
    target_->set_output_conversion(*value);
    return true;
  }

  return false;
}
