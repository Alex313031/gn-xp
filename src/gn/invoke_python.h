// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_INVOKE_PYTHON_H_
#define TOOLS_GN_INVOKE_PYTHON_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "gn/err.h"

class BuildSettings;

namespace internal {

bool InvokePython(const BuildSettings* build_settings,
                  const base::FilePath& python_script_path,
                  const std::string& python_script_extra_args,
                  const base::FilePath& output_path,
                  bool quiet,
                  Err* err);

}  // namespace internal

#endif  // TOOLS_UTILS_INVOKE_PYTHON_H_
