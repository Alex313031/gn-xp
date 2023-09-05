// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "gn/input_file.h"
#include "gn/scope.h"
#include "gn/source_file.h"
#include "gn/value.h"

#ifndef TOOLS_GN_INPUT_FALLBACK_HANDLER_H_
#define TOOLS_GN_INPUT_FALLBACK_HANDLER_H_

class InputLoadResult {
 public:
  virtual Value Execute(Scope *scope, Err *err) const = 0;
  virtual ~InputLoadResult() = default;
};

// Provides a loader for fallback input build files, given an input GN build
// file. Only build files, not imports, are supported, though this may change
// in the future.
class InputAlternateLoader {
 public:
  virtual std::unique_ptr<InputLoadResult> TryLoadAlternateFor(const SourceFile& file, InputFile *input_file) const = 0;
  virtual ~InputAlternateLoader() = default;
};

#endif  // TOOLS_GN_INPUT_FALLBACK_HANDLER_H_
