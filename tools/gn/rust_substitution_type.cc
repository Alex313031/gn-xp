// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/rust_substitution_type.h"

#include <stddef.h>
#include <stdlib.h>

#include "tools/gn/err.h"
#include "tools/gn/substitution_type.h"

const SubstitutionTypes RustSubstitutions = {
    &RustSubstitutionRustFlags,
    &RustSubstitutionRustEnv,
    &RustSubstitutionCrateName,
    &RustSubstitutionRustDeps,
    &RustSubstitutionRustRlibs,
};

// Valid for compiler tools.
const Substitution RustSubstitutionRustFlags = {"{{rustflags}}", "rustflags"};
const Substitution RustSubstitutionRustEnv = {"{{rustenv}}", "rustenv"};
const Substitution RustSubstitutionCrateName = {"{{crate_name}}", "crate_name"};
const Substitution RustSubstitutionRustDeps = {"{{rustdeps}}", "rustdeps"};
const Substitution RustSubstitutionRustRlibs = {"{{rlibs}}", "rlibs"};

bool IsValidRustSubstitution(const Substitution* type) {
  return IsValidToolSubstitution(type) || IsValidSourceSubstitution(type) ||
         type == &RustSubstitutionRustFlags ||
         type == &RustSubstitutionRustEnv ||
         type == &RustSubstitutionCrateName ||
         type == &RustSubstitutionRustDeps ||
         type == &RustSubstitutionRustRlibs;
}