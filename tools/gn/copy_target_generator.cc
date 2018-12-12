// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/copy_target_generator.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"
#include "tools/gn/variables.h"

CopyTargetGenerator::CopyTargetGenerator(Target* target,
                                         Scope* scope,
                                         const FunctionCallNode* function_call,
                                         Err* err)
    : TargetGenerator(target, scope, function_call, err) {}

CopyTargetGenerator::~CopyTargetGenerator() = default;

void CopyTargetGenerator::DoRun() {
  target_->set_output_type(Target::COPY_FILES);

  if (!FillSources())
    return;
  if (!FillOutputs(true))
    return;

  // Only do this validation here if these vars were not opaque, otherwise defer
  // the work to DoFinish.
  if (target_->unfinished_vars().find(variables::kSources) ==
      target_->unfinished_vars().end()) {
    if (target_->sources().empty()) {
      *err_ = Err(
          function_call_, "Empty sources for copy command.",
          "You have to specify at least one file to copy in the \"sources\".");
      return;
    }
  }
  if (target_->unfinished_vars().find(variables::kOutputs) ==
      target_->unfinished_vars().end()) {
    if (target_->action_values().outputs().list().size() != 1) {
      *err_ = Err(
          function_call_, "Copy command must have exactly one output.",
          "You must specify exactly one value in the \"outputs\" array for the "
          "destination of the copy\n(see \"gn help copy\"). If there are "
          "multiple sources to copy, use source expansion\n(see \"gn help "
          "source_expansion\").");
      return;
    }
  }
}

void CopyTargetGenerator::DoFinish(Target::UnfinishedVars& unfinished_vars) {
  for (const auto& var : unfinished_vars) {
    if (var.first == variables::kSources) {
      if (!FillSources())
        return;
    } else if (var.first == variables::kOutputs) {
      if (!FillOutputs(true))
        return;
    }
  }
  unfinished_vars.erase(variables::kSources);
  unfinished_vars.erase(variables::kOutputs);

  if (target_->sources().empty()) {
    *err_ = Err(
        function_call_, "Empty sources for copy command.",
        "You have to specify at least one file to copy in the \"sources\".");
    return;
  }
  if (target_->action_values().outputs().list().size() != 1) {
    *err_ = Err(
        function_call_, "Copy command must have exactly one output.",
        "You must specify exactly one value in the \"outputs\" array for the "
        "destination of the copy\n(see \"gn help copy\"). If there are "
        "multiple sources to copy, use source expansion\n(see \"gn help "
        "source_expansion\").");
    return;
  }
}
