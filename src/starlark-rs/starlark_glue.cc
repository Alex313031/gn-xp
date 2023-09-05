// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <stdbool.h>
#include <stdio.h>
#include <cxx.h>
#include "cxxgen.h"
#include "cxxgen1.h"
#include "starlark-rs/stargn_main.h"
#include "starlark-rs/starlark_glue.h"

#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/input_alternate_loader.h"
#include "gn/parse_tree.h"
#include "gn/token.h"

Value StarlarkLoadResult::Execute(Scope *scope, Err *err) const {
  // TODO: error handling?
  exec_starlark(std::make_unique<GNExecContext>(scope), std::move(*parse_result_));
  return Value();
}

std::unique_ptr<InputLoadResult> StarlarkInputLoader::TryLoadAlternateFor(const SourceFile& source_file, InputFile *input_file) const {
    base::FilePath starlark_input_path =
        build_settings_->GetFullPath(source_file).ReplaceExtension(".stargn");
    if (!input_file->Load(starlark_input_path)) {
        // TODO: return Err maybe?
        return NULL;
    }
    return std::make_unique<StarlarkLoadResult>(parse_starlark(*input_file, starlark_input_path.value()));
}
