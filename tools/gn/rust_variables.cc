// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_variables.h"

namespace variables {

// Rust target variables ------------------------------------------------------

const char kRustCrateName[] = "crate_name";
const char kRustCrateName_HelpShort[] =
    "crate_name: [string] The name for the compiled crate.";
const char kRustCrateName_Help[] =
    R"(crate_name: [string] The name for the compiled crate.

  If crate_name is not set, then this rule will use the target name.
)";

const char kRustCrateType[] = "crate_type";
const char kRustCrateType_HelpShort[] =
    "crate_type: [string] The type of linkage to use.";
const char kRustCrateType_Help[] =
    R"(crate_type: [string] The type of linkage to use.

  Options for this field are "rlib", "dylib", "cdylib", "staticlib",
  and "proc-macro". This field sets the `crate-type` attribute for the `rustc`
  tool, as well as the appropiate output extension in the
  `rust_output_extension` attribute. Since outputs must be explicit, the `lib`
  crate type (where the Rust compiler produces what it thinks is the
  appropriate library type) is not supported.
)";

const char kRustCrateRoot[] = "crate_root";
const char kRustCrateRoot_HelpShort[] =
    "crate_root: [string] The root source file for a binary or library.";
const char kRustCrateRoot_Help[] =
    R"(crate_root: [string] The root source file for a binary or library.

  This file is usually the `main.rs` or `lib.rs` for binaries and libraries,
  respectively. 

  If crate_root is not set, then this rule will look for a lib.rs file (or
  main.rs for rust_executable) or the single file in sources if sources
  contains only one file.
)";

void InsertRustVariables(VariableInfoMap* info_map) {
  info_map->insert(std::make_pair(
      kRustCrateRoot,
      VariableInfo(kRustCrateRoot_HelpShort, kRustCrateRoot_Help)));
  info_map->insert(std::make_pair(
      kRustCrateType,
      VariableInfo(kRustCrateType_HelpShort, kRustCrateType_Help)));
  info_map->insert(std::make_pair(
      kRustCrateRoot,
      VariableInfo(kRustCrateRoot_HelpShort, kRustCrateRoot_Help)));
}

}  // namespace variables
