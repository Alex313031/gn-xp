// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_TOOLS_H_
#define TOOLS_GN_NINJA_TOOLS_H_

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "gn/err.h"
#include "gn/version.h"

std::optional<Version> GetNinjaVersion(const base::FilePath& ninja_executable);

bool InvokeNinjaRestatTool(const base::FilePath& ninja_executable,
                           const base::FilePath& build_dir,
                           const std::vector<base::FilePath>& files_to_restat,
                           Err* err);

bool InvokeNinjaCleanDeadTool(const base::FilePath& ninja_executable,
                              const base::FilePath& build_dir,
                              Err* err);

bool InvokeNinjaRecompactTool(const base::FilePath& ninja_executable,
                              const base::FilePath& build_dir,
                              Err* err);

#endif // TOOLS_GN_NINJA_TOOLS_H_
