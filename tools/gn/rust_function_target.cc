// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_functions.h"

#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

namespace functions {

// Rust target variables ------------------------------------------------------

// TODO(juliehockett): write help
const char kRustExecutable[] = "rust_executable";
const char kRustExecutable_HelpShort[] =
    "rust_executable: Declare a Rust executable target.";
const char kRustExecutable_Help[] =
    R"(rust_executable: Declare a Rust executable target.
)";
Value RunRustExecutable(Scope* scope,
                        const FunctionCallNode* function,
                        const std::vector<Value>& args,
                        BlockNode* block,
                        Err* err) {
  return ExecuteGenericTarget(functions::kRustExecutable, scope, function, args,
                              block, err);
}

const char kRustLibrary[] = "rust_library";
const char kRustLibrary_HelpShort[] =
    "rust_executable: Declare a Rust library target.";
const char kRustLibrary_Help[] =
    R"(rust_executable: Declare a Rust library target.
)";
Value RunRustLibrary(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     BlockNode* block,
                     Err* err) {
  return ExecuteGenericTarget(functions::kRustLibrary, scope, function, args,
                              block, err);
}

void InsertRustFunctions(FunctionInfoMap* info_map) {
  info_map->insert(
      std::make_pair(kRustExecutable,
                     FunctionInfo(&RunRustExecutable, kRustExecutable_HelpShort,
                                  kRustExecutable_Help, true)));
  info_map->insert(std::make_pair(
      kRustLibrary, FunctionInfo(&RunRustLibrary, kRustLibrary_HelpShort,
                                 kRustLibrary_Help, true)));
}

}  // namespace functions
