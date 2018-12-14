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
    const ParseNode* function_call,
    Target::OutputType type,
    Err* err)
    : TargetGenerator(target, scope, function_call, err),
      output_type_(type),
      contents_defined_(false),
      data_keys_defined_(false) {}

WriteDataTargetGenerator::~WriteDataTargetGenerator() = default;

void WriteDataTargetGenerator::DoRun() {
  if (!FillOutputs(false))
    return;
  if (target_->action_values().outputs().list().size() != 1) {
    *err_ = Err(
        function_call_, "write_data command must have exactly one output.",
        "You must specify exactly one value in the \"outputs\" array for the "
        "destination of the write\n(see \"gn help write_data\").");
    return;
  }

  if (!FillContents())
    return;
  if (!FillDataKeys())
    return;

  // One of data and data_keys should be defined.
  if (!contents_defined_ && !data_keys_defined_) {
    *err_ = Err(function_call_, "Either contents or data_keys should be set.",
                "write_data wants some sort of data to write.");
    return;
  }

  if (!FillRebase())
    return;
  if (!FillWalkKeys())
    return;

  if (!FillOutputConversion())
    return;
}

bool WriteDataTargetGenerator::FillContents() {
  const Value* value = scope_->GetValue(variables::kWriteValueContents, true);
  if (!value)
    return true;
  target_->set_contents(*value);
  contents_defined_ = true;
  return true;
}

bool WriteDataTargetGenerator::ContentsDefined(const base::StringPiece& name) {
  if (contents_defined_) {
    *err_ =
        Err(function_call_, name.as_string() + " won't be used.",
            "write_data is defined on this target, and so setting " +
                name.as_string() +
                " will have no effect as no metdata collection will occur.");
    return true;
  }
  return false;
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

bool WriteDataTargetGenerator::FillRebase() {
  const Value* value = scope_->GetValue(variables::kRebase, true);
  if (!value)
    return true;
  if (ContentsDefined(variables::kRebase))
    return false;
  if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
    return false;
  target_->set_rebase(value->boolean_value());
  return true;
}

bool WriteDataTargetGenerator::FillDataKeys() {
  const Value* value = scope_->GetValue(variables::kDataKeys, true);
  if (!value)
    return true;
  if (ContentsDefined(variables::kDataKeys))
    return false;
  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  for (const Value& v : value->list_value()) {
    // Keys must be strings.
    if (!v.VerifyTypeIs(Value::STRING, err_))
      return false;
    target_->data_keys().push_back(v.string_value());
  }

  data_keys_defined_ = true;
  return true;
}

bool WriteDataTargetGenerator::FillWalkKeys() {
  const Value* value = scope_->GetValue(variables::kWalkKeys, true);
  // If we define this and contents, that's an error.
  if (value && ContentsDefined(variables::kWalkKeys))
    return false;

  // If we don't define it, we want the default value which is a list
  // containing the empty string.
  if (!value) {
    target_->walk_keys().push_back("");
    return true;
  }

  // Otherwise, pull and validate the specified value.
  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;
  for (const Value& v : value->list_value()) {
    // Keys must be strings.
    if (!v.VerifyTypeIs(Value::STRING, err_))
      return false;
    target_->walk_keys().push_back(v.string_value());
  }
  return true;
}
