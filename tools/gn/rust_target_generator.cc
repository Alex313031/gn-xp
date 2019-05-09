// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_target_generator.h"

#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/rust_variables.h"
#include "tools/gn/scope.h"
#include "tools/gn/target.h"
#include "tools/gn/value_extractors.h"

RustTargetGenerator::RustTargetGenerator(Target* target,
                                         Scope* scope,
                                         const FunctionCallNode* function_call,
                                         Err* err)
    : TargetGenerator(target, scope, function_call, err) {}

RustTargetGenerator::~RustTargetGenerator() = default;

void RustTargetGenerator::DoRun() {
  // source_set targets don't need any special Rust handling.
  if (target_->output_type() == Target::SOURCE_SET)
    return;

  // Check that this type of target is Rust-supported.
  if (target_->output_type() != Target::EXECUTABLE &&
      target_->output_type() != Target::SHARED_LIBRARY &&
      target_->output_type() != Target::RUST_LIBRARY &&
      target_->output_type() != Target::STATIC_LIBRARY) {
    // Only valid rust output types.
    *err_ = Err(function_call_,
                "Target type \"" +
                    std::string(Target::GetStringForOutputType(
                        target_->output_type())) +
                    "\" is not supported for Rust compilation.",
                "Supported target types are \"executable\", "
                "\"shared_library\", \"static_library\", or "
                "\"source_set\".");
    return;
  }

  if (!FillCrateName())
    return;

  if (!FillCrateType())
    return;

  if (!FillCrateRoot())
    return;

  if (!FillEdition())
    return;

  if (!FillRenamedDeps())
    return;
}

bool RustTargetGenerator::FillCrateName() {
  const Value* value = scope_->GetValue(variables::kRustCrateName, true);
  if (!value) {
    // The target name will be used.
    target_->rust_values().crate_name() = target_->label().name();
    return true;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  target_->rust_values().crate_name() = std::move(value->string_value());
  return true;
}

bool RustTargetGenerator::FillCrateType() {
  const Value* value = scope_->GetValue(variables::kRustCrateType, true);
  if (!value) {
    // Non-shared_library targets shouldn't set this, so that's okay.
    if (target_->output_type() != Target::SHARED_LIBRARY)
      return true;
    // But require shared_library targets to tell us what they want.
    *err_ = Err(function_call_,
                "Must set \"crate_type\" on a Rust \"shared_library\".",
                "\"crate_type\" must be one of \"dylib\", \"cdylib\", or "
                "\"proc-macro\".");
    return false;
  }

  if (target_->output_type() != Target::SHARED_LIBRARY) {
    *err_ = Err(
        value->origin(),
        "\"crate_type\" automatically inferred for non-shared Rust targets.",
        "Setting it here has no effect.");
    return false;
  }

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  if (value->string_value() == "dylib") {
    target_->rust_values().set_crate_type(RustValues::CRATE_DYLIB);
    return true;
  }
  if (value->string_value() == "cdylib") {
    target_->rust_values().set_crate_type(RustValues::CRATE_CDYLIB);
    return true;
  }
  if (value->string_value() == "proc-macro") {
    target_->rust_values().set_crate_type(RustValues::CRATE_PROC_MACRO);
    return true;
  }

  *err_ = Err(value->origin(),
              "Inadmissible crate type \"" + value->string_value() + "\".",
              "\"crate_type\" must be one of \"dylib\", \"cdylib\", or "
              "\"proc-macro\" for a \"shared_library\".");
  return false;
}

bool RustTargetGenerator::FillCrateRoot() {
  const Value* value = scope_->GetValue(variables::kRustCrateRoot, true);
  if (!value) {
    // If there's only one source, use that.
    if (target_->sources().size() == 1) {
      target_->rust_values().set_crate_root(target_->sources()[0]);
      return true;
    }
    // Otherwise, see if "lib.rs" or "main.rs" (as relevant) are in sources.
    std::string to_find =
        target_->output_type() == Target::EXECUTABLE ? "main.rs" : "lib.rs";
    for (auto& source : target_->sources()) {
      if (source.GetName() == to_find) {
        target_->rust_values().set_crate_root(source);
        return true;
      }
    }
    *err_ = Err(function_call_, "Missing \"crate_root\" and missing \"" +
                                    to_find + "\" in sources.");
    return false;
  }

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  SourceFile dest;
  if (!ExtractRelativeFile(scope_->settings()->build_settings(), *value,
                           scope_->GetSourceDir(), &dest, err_))
    return false;

  target_->rust_values().set_crate_root(dest);
  return true;
}

bool RustTargetGenerator::FillEdition() {
  const Value* value = scope_->GetValue(variables::kRustEdition, true);
  if (!value) {
    *err_ = Err(function_call_, "Missing \"edition\" in Rust target.");
    return false;
  }

  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  target_->rust_values().edition() = std::move(value->string_value());
  return true;
}

bool RustTargetGenerator::FillRenamedDeps() {
  const Value* value = scope_->GetValue(variables::kRustRenamedDeps, true);
  if (!value)
    return true;

  if (!value->VerifyTypeIs(Value::LIST, err_))
    return false;

  for (const Value& pair : value->list_value()) {
    if (!pair.VerifyTypeIs(Value::LIST, err_))
      return false;
    if (pair.list_value().size() != 2U) {
      *err_ = Err(
          pair.origin(),
          "Each element in a \"renamed_deps\" list must be a two-element list.",
          "The first element should indicate the new name for the crate, and "
          "the second should indicate the relevant label.");
      return false;
    }

    // Both elements in the two-element list should be strings, but the label
    // resolver checks the second one.
    if (!pair.list_value()[0].VerifyTypeIs(Value::STRING, err_))
      return false;

    Label dep_label =
        Label::Resolve(scope_->GetSourceDir(), ToolchainLabelForScope(scope_),
                       pair.list_value()[1], err_);

    if (err_->has_error())
      return false;

    // Insert into the renamed_deps map.
    target_->rust_values().renamed_deps().insert(std::pair<Label, std::string>(
        std::move(dep_label), std::move(pair.list_value()[0].string_value())));
  }

  return true;
}
