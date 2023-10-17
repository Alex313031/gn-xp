// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_OUTPUTS_WRITER_H_
#define TOOLS_GN_NINJA_OUTPUTS_WRITER_H_

#include "gn/err.h"
#include "gn/target.h"

#include <vector>

class Builder;
class BuildSettings;
class StringOutputBuffer;

class NinjaOutputsWriter {
 public:
  static bool RunAndWriteFiles(const BuildSettings* build_setting,
                               const Builder& builder,
                               const std::string& file_name,
                               const std::string& dir_filter_string,
                               bool quiet,
                               Err* err);

 private:
  static StringOutputBuffer GenerateOutputs(
      const BuildSettings* build_settings,
      std::vector<const Target*>& all_targets);
};

#endif
