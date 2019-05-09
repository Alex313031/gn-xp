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
    "crate_type: [string] The type of linkage to use on a shared_library.";
const char kRustCrateType_Help[] =
    R"(crate_type: [string] The type of linkage to use on a shared_library.

  Options for this field are "cdylib", "staticlib", "proc-macro", and "dylib".
  This field sets the `crate-type` attribute for the `rustc` tool on static
  libraries, as well as the appropiate output extension in the
  `rust_output_extension` attribute. Since outputs must be explicit, the `lib`
  crate type (where the Rust compiler produces what it thinks is the
  appropriate library type) is not supported.

  It should be noted that the "dylib" crate type in Rust is unstable in the set
  of symbols it exposes, and most usages today are potentially wrong and will
  be broken in the future.

  Static libraries, rust libraries, and executables have this field set
  automatically.
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

const char kRustEdition[] = "edition";
const char kRustEdition_HelpShort[] =
    "edition: [string] The rustc edition to use in compiliation.";
const char kRustEdition_Help[] =
    R"(edition: [string] The rustc edition to use in compiliation.

  This indicates the compiler edition to use in compilition. Should be a value
  like "2015" or "2018", indiicating the appropriate value to pass to the
  `--edition=<>` flag in rustc.
)";

const char kRustRenamedDeps[] = "renamed_deps";
const char kRustRenamedDeps_HelpShort[] =
    "renamed_deps: [list of lists] List of crate-dependency pairs.";
const char kRustRenamedDeps_Help[] =
    R"(renamed_deps: [list of lists] List of crate-dependency pairs.

  A list of two-element lists, with the first element in each sublist
  indicating the renamed crate and the second element specifying the label of
  the dependency producing the relevant binary.

  All dependencies listed in this field *must* be listed as deps of the target.

  ```
  executable("foo") {
    sources = [ "main.rs" ]
    deps = [ "//bar" ]
  }
  ```

  This target would compile the `foo` crate with the following `extern` flag:
  `rustc ...command... --extern bar=<build_out_dir>/obj/bar`

  ```
  executable("foo") {
    sources = [ "main.rs" ]
    deps = [ ":bar" ]
    renamed_deps = [ "bar_renamed", ":bar" ]
  }
  ```
  
  With the addition of `renamed_deps`, above target would instead compile with:
  `rustc ...command... --extern bar_renamed=<build_out_dir>/obj/bar`

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
  info_map->insert(std::make_pair(
      kRustEdition, VariableInfo(kRustEdition_HelpShort, kRustEdition_Help)));
  info_map->insert(std::make_pair(
      kRustRenamedDeps,
      VariableInfo(kRustRenamedDeps_HelpShort, kRustRenamedDeps_Help)));
}

}  // namespace variables
