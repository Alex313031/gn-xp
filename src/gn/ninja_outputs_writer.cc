// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_outputs_writer.h"

#include <algorithm>
#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "gn/builder.h"
#include "gn/commands.h"
#include "gn/deps_iterator.h"
#include "gn/exec_process.h"
#include "gn/filesystem_utils.h"
#include "gn/settings.h"
#include "gn/string_output_buffer.h"

StringOutputBuffer NinjaOutputsWriter::GenerateOutputs(
    const MapType& outputs_map,
    const BuildSettings* build_settings) {
  Label default_toolchain_label;
  if (!outputs_map.empty()) {
    default_toolchain_label =
        outputs_map.begin()->first->settings()->default_toolchain_label();
  }

  StringOutputBuffer out;

  // Sort the targets according to their human visible labels first.
  struct TargetLabelPair {
    TargetLabelPair(const Target* target, const Label& default_toolchain_label)
        : target(target),
          label(std::make_unique<std::string>(
              target->label().GetUserVisibleName(default_toolchain_label))) {}

    const Target* target;
    std::unique_ptr<std::string> label;

    bool operator<(const TargetLabelPair& other) const {
      return *label < *other.label;
    }
  };
  std::vector<TargetLabelPair> sorted_pairs;
  sorted_pairs.reserve(outputs_map.size());

  for (const auto& output_pair : outputs_map)
    sorted_pairs.emplace_back(output_pair.first, default_toolchain_label);

  std::sort(sorted_pairs.begin(), sorted_pairs.end());

  // Helper lambda to append a path to the current line, performing escaping
  // of \\, \n and \t if needed.
  auto append_path = [&out](const std::string& path) {
    size_t escapes = 0;
    for (char ch : path) {
      escapes += (ch == '\\' || ch == '\t' || ch == '\n');
    }
    if (escapes == 0) {
      out << "\t" << path;
      return;
    }
    std::string escaped;
    escaped.reserve(path.size() + escapes);
    for (char ch : path) {
      switch (ch) {
        case '\t':
          escaped.append("\\t", 2);
          break;
        case '\n':
          escaped.append("\\n", 2);
          break;
        case '\\':
          escaped.append("\\\\", 2);
          break;
        default:
          escaped.push_back(ch);
      }
    }
    out << "\t" << escaped;
  };

  for (const auto& pair : sorted_pairs) {
    const Target* target = pair.target;
    const std::string& label = *pair.label;

    auto it = outputs_map.find(target);
    CHECK(it != outputs_map.end());

    out << label;
    for (const auto& output : it->second)
      append_path(output.value());

    out << "\n";
  }
  return out;
}

bool NinjaOutputsWriter::RunAndWriteFiles(const MapType& outputs_map,
                                          const BuildSettings* build_settings,
                                          const std::string& file_name,
                                          Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null()) {
    return false;
  }

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  StringOutputBuffer outputs = GenerateOutputs(outputs_map, build_settings);
  if (!outputs.ContentsEqual(output_path)) {
    if (!outputs.WriteToFile(output_path, err)) {
      return false;
    }
  }

  return true;
}
