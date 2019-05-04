// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_target_generator.h"

#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/rust_functions.h"
#include "tools/gn/rust_variables.h"
#include "tools/gn/scope.h"
#include "tools/gn/target.h"
#include "tools/gn/value_extractors.h"

RustTargetGenerator::RustTargetGenerator(Target* target,
                                         Scope* scope,
                                         const FunctionCallNode* function_call,
                                         const char* type,
                                         Err* err)
    : TargetGenerator(target, scope, function_call, err), output_type_(type) {}

RustTargetGenerator::~RustTargetGenerator() = default;

void RustTargetGenerator::DoRun() {
  target_->set_output_type(output_type_);

  if (!FillSources())
    return;
  if (!FillCrateType())
    return;
  if (!FillCrateRoot())
    return;
}

bool RustTargetGenerator::FillCrateType() {
  const Value* value = scope_->GetValue(variables::kRustCrateType, true);
  if (!value) {
    // Executables will always be "bin" type.
    if (output_type_ == functions::kRustExecutable) {
      target_->rust_values().crate_type() = std::move("bin");
      return true;
    }
    // But require libs to tell us what they want.
    *err_ =
        Err(function_call_, "Must set \"crate_type\" on a \"rust_library\".",
            "\"crate_type\" must be one of \"lib\", \"rlib\", \"dylib\", "
            "\"cdylib\", \"staticlib\", or \"proc-macro\".");
    return false;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;

  if ((output_type_ == functions::kRustExecutable &&
       value->string_value() == "bin") ||
      (output_type_ == functions::kRustLibrary &&
       (value->string_value() == "lib" || value->string_value() == "rlib" ||
        value->string_value() == "dylib" || value->string_value() == "cdylib" ||
        value->string_value() == "staticlib" ||
        value->string_value() == "proc-macro"))) {
    target_->rust_values().crate_type() = std::move(value->string_value());
    return true;
  }

  *err_ = Err(value->origin(),
              "Inadmissible crate type " + value->string_value() + ".",
              "\"crate_type\" must be one of \"lib\", \"rlib\", \"dylib\", "
              "\"cdylib\", "
              "\"staticlib\", or \"proc-macro\" for a \"rust_library\", or "
              "\"bin\" for "
              "a \"rust_executable\".");
  return false;
}

bool RustTargetGenerator::FillCrateRoot() {
  const Value* value = scope_->GetValue(variables::kRustCrateRoot, true);
  if (!value) {
    // If there's only one source, use that.
    if (target_->sources().size() == 1) {
      target_->rust_values().set_crate_root(&target_->sources()[0]);
      return true;
    }
    // Otherwise, see if "lib.rs" or "main.rs" (as relevant) are in sources.
    std::string to_find =
        output_type_ == functions::kRustExecutable ? "main.rs" : "lib.rs";
    for (auto& source : target_->sources()) {
      if (source.GetName() == to_find) {
        target_->rust_values().set_crate_root(&source);
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

  // Check if this file is already in sources.
  for (const auto& source : target_->sources()) {
    if (source == dest) {
      target_->rust_values().set_crate_root(&dest);
      return true;
    }
  }

  // It's not in sources, so we should add it.
  target_->sources().push_back(std::move(dest));
  target_->rust_values().set_crate_root(&target_->sources().back());
  return true;
}