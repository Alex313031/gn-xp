// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_substitution_type.h"

#include <stddef.h>
#include <stdlib.h>

#include "tools/gn/err.h"
#include "tools/gn/substitution_type.h"

const SubstitutionTypes RustSubstitutions = {
    &RustSubstitutionCrateName,       &RustSubstitutionCrateType,
    &RustSubstitutionOutputExtension, &RustSubstitutionOutputPrefix,
    &RustSubstitutionRustDeps,        &RustSubstitutionRustFlags,
    &RustSubstitutionRustEnv,         &RustSubstitutionRustRlibs,
};

// Valid for Rust tools.
const Substitution RustSubstitutionCrateName = {"{{crate_name}}", "crate_name"};
const Substitution RustSubstitutionCrateType = {"{{crate_type}}", "crate_type"};
const Substitution RustSubstitutionOutputExtension = {
    "{{rustc_output_extension}}", "rustc_output_extension"};
const Substitution RustSubstitutionOutputPrefix = {"{{rustc_output_prefix}}",
                                                   "rustc_output_prefix"};
const Substitution RustSubstitutionRustDeps = {"{{rustdeps}}", "rustdeps"};
const Substitution RustSubstitutionRustEnv = {"{{rustenv}}", "rustenv"};
const Substitution RustSubstitutionRustFlags = {"{{rustflags}}", "rustflags"};
const Substitution RustSubstitutionRustRlibs = {"{{rlibs}}", "rlibs"};

bool IsValidRustSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) || IsValidSourceSubstitution(type) ||
         type == &RustSubstitutionCrateName ||
         type == &RustSubstitutionCrateType ||
         type == &RustSubstitutionOutputExtension ||
         type == &RustSubstitutionOutputPrefix ||
         type == &RustSubstitutionRustDeps ||
         type == &RustSubstitutionRustEnv ||
         type == &RustSubstitutionRustFlags ||
         type == &RustSubstitutionRustRlibs;
}