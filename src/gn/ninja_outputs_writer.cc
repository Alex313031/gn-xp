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

namespace {

void AddTargetDependencies(const Target* target, TargetSet* deps) {
  for (const auto& pair : target->GetDeps(Target::DEPS_LINKED)) {
    if (deps->add(pair.ptr)) {
      AddTargetDependencies(pair.ptr, deps);
    }
  }
}

// Filters targets according to filter string; Will also recursively
// add dependent targets.
bool FilterTargets(const BuildSettings* build_settings,
                   std::vector<const Target*>& all_targets,
                   std::vector<const Target*>* targets,
                   const std::string& dir_filter_string,
                   Err* err) {
  if (dir_filter_string.empty()) {
    *targets = all_targets;
  } else {
    targets->reserve(all_targets.size());
    std::vector<LabelPattern> filters;
    if (!commands::FilterPatternsFromString(build_settings, dir_filter_string,
                                            &filters, err)) {
      return false;
    }
    commands::FilterTargetsByPatterns(all_targets, filters, targets);

    TargetSet target_set(targets->begin(), targets->end());
    for (const auto* target : *targets)
      AddTargetDependencies(target, &target_set);

    targets->clear();
    targets->insert(targets->end(), target_set.begin(), target_set.end());
  }

  // Sort the list of targets per-label to get a consistent ordering of them
  // in the generated project (and thus stability of the file generated).
  std::sort(targets->begin(), targets->end(),
            [](const Target* a, const Target* b) {
              return a->label().name() < b->label().name();
            });

  return true;
}

}  // namespace

StringOutputBuffer NinjaOutputsWriter::GenerateOutputs(
    const BuildSettings* build_settings,
    std::vector<const Target*>& all_targets) {
  Label default_toolchain_label;
  if (!all_targets.empty())
    default_toolchain_label =
        all_targets[0]->settings()->default_toolchain_label();

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
  sorted_pairs.reserve(all_targets.size());

  for (const Target* target : all_targets)
    sorted_pairs.emplace_back(target, default_toolchain_label);

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

    std::vector<std::string> outputs;

    // For groups, just print the dependency output file for it
    // (which will be either a stamp file or a phony alias).
    if (target->output_type() == Target::GROUP) {
      outputs.push_back(target->dependency_output_file().value());

      // See FillInOutputs() in desc_builder.cc
    } else if (target->output_type() != Target::SOURCE_SET &&
               target->output_type() != Target::BUNDLE_DATA) {
      Err err;
      std::vector<SourceFile> output_sources;
      if (!target->GetOutputsAsSourceFiles(LocationRange(), true,
                                           &output_sources, &err)) {
        err.PrintToStdout();
        return {};
      }
      for (auto& output : output_sources) {
        outputs.push_back(
            RebasePath(output.value(), build_settings->build_dir()));
      }
    }

    // See FillInSourceOutputs() in desc_builder.cc
    if (target->IsBinary() || target->output_type() == Target::ACTION_FOREACH ||
        (target->output_type() == Target::COPY_FILES &&
         !target->action_values().outputs().required_types().empty())) {
      for (const auto& source : target->sources()) {
        std::vector<OutputFile> output_files;
        const char* tool_name = Tool::kToolNone;
        if (target->GetOutputFilesForSource(source, &tool_name,
                                            &output_files)) {
          for (auto& output : output_files)
            outputs.push_back(std::move(output.value()));
        }
      }
    }

    out << label;

    for (const auto& output : outputs)
      append_path(output);

    out << "\n";
  }
  return out;
}

bool NinjaOutputsWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                          const Builder& builder,
                                          const std::string& file_name,
                                          const std::string& dir_filter_string,
                                          bool quiet,
                                          Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null()) {
    return false;
  }

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();
  std::vector<const Target*> targets;
  if (!FilterTargets(build_settings, all_targets, &targets, dir_filter_string,
                     err)) {
    return false;
  }

  StringOutputBuffer outputs = GenerateOutputs(build_settings, targets);
  if (!outputs.ContentsEqual(output_path)) {
    if (!outputs.WriteToFile(output_path, err)) {
      return false;
    }
  }

  return true;
}
