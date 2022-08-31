// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMPILE_COMMANDS_WRITER_H_
#define TOOLS_GN_COMPILE_COMMANDS_WRITER_H_

#include <vector>

#include "gn/err.h"
#include "gn/label_pattern.h"
#include "gn/target.h"

class Builder;
class BuildSettings;

class CompileCommandsWriter {
 public:
  // Encapsulates the different ways of specifying which targets to generate for
  // compile commands.
  // 
  // The original way to specify this was the command-line switch which matched
  // targets in any directory with a given taget_name.
  //
  // Later, a setting in the .gn file was added and this was converted to label
  // patterns for consistency with other GN designs. If the command-line switch
  // is specified, it takes precedence.
  //
  // TODO remove the legacy target_name behavior and use vector<LabelPattern>
  // consistently. This struct can then be removed.
  struct Filter {
    explicit Filter(std::string legacy_filters)
        : mode(kLegacy), legacy_target_filters(std::move(legacy_filters)) {}
    explicit Filter(std::vector<LabelPattern> patterns)
        : mode(kLabelPattern), patterns(std::move(patterns)) {}

    enum Mode { kLegacy, kLabelPattern };
    Mode mode;

    std::string legacy_target_filters;   // When mode == kLegacy.
    std::vector<LabelPattern> patterns;  // When mode == kLabelPattern.
  };

  // Writes a compilation databased to the given file name consisting of the
  // recursive dependencies of all targets that match or are dependencies of
  // targets that match any given pattern.
  static bool RunAndWriteFiles(const BuildSettings* build_setting,
                               const Builder& builder,
                               const base::FilePath& output_path,
                               const std::vector<LabelPattern>& patterns,
                               Err* err);

  // Writes a compilation database using the legacy way of specifying which
  // targets to output. This format uses a comma-separated list of target names
  // ("target_name1,target_name2...") which are matched against targets in any
  // directory. Then the recursive dependencies of these deps are collected.
  //
  // TODO remove this legacy target_name behavior and use vector<LabelPattern>
  // version consistently.
  static bool RunAndWriteFilesLegacyFilters(const BuildSettings* build_setting,
                               const Builder& builder,
                               const base::FilePath& output_path,
                               const std::string& target_filters,
                               Err* err);

  static std::string RenderJSON(const BuildSettings* build_settings,
                                std::vector<const Target*>& all_targets);

  // Does a depth-first search of the graph starting at each target that matches
  // the given pattern, and collects all recursive dependencies of those targets.
  static std::vector<const Target*> CollectDepsOfMatches(
      const std::vector<const Target*>& all_targets,
      const std::vector<LabelPattern>& patterns);

  // Performs the legacy target_name filtering.
  static std::vector<const Target*> FilterTargets(
      const std::vector<const Target*>& all_targets,
      const std::set<std::string>& target_filters_set);

 private:
  // This function visits the deps graph of a target in a DFS fashion.
  static void VisitDeps(const Target* target, TargetSet* visited);
};

#endif  // TOOLS_GN_COMPILE_COMMANDS_WRITER_H_
