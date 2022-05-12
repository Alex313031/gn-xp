// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_binary_target_writer.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "gn/config_values_extractors.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_c_binary_target_writer.h"
#include "gn/ninja_rust_binary_target_writer.h"
#include "gn/ninja_target_command_util.h"
#include "gn/ninja_utils.h"
#include "gn/settings.h"
#include "gn/string_utils.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/variables.h"

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out) {}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() = default;

void NinjaBinaryTargetWriter::Run() {
  if (target_->source_types_used().RustSourceUsed()) {
    NinjaRustBinaryTargetWriter writer(target_, out_);
    writer.Run();
    return;
  }

  NinjaCBinaryTargetWriter writer(target_, out_);
  writer.Run();
}

void NinjaBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  ClassifiedDeps classified_deps = GetClassifiedDeps();

  // The classifier should never put extra object files in a source sets: any
  // source sets that we depend on should appear in our non-linkable deps
  // instead.
  DCHECK(classified_deps.extra_object_files.empty());

  std::vector<OutputFile> order_only_deps;
  for (auto* dep : classified_deps.non_linkable_deps)
    order_only_deps.push_back(dep->dependency_output_file());

  WriteStampForTarget(object_files, order_only_deps);
}

void NinjaBinaryTargetWriter::WriteLinkerFlags(
    std::ostream& out,
    const Tool* tool,
    const SourceFile* optional_def_file) {
  // First any ldflags
  WriteCustomLinkerFlags(out, tool);
  // Then the library search path
  WriteLibrarySearchPath(out, tool);

  if (optional_def_file) {
    out_ << " /DEF:";
    path_output_.WriteFile(out, *optional_def_file);
  }
}

void NinjaBinaryTargetWriter::WriteFrameworks(std::ostream& out,
                                              const Tool* tool) {
  // Frameworks that have been recursively pushed through the dependency tree.
  FrameworksWriter writer(tool->framework_switch());
  const auto& all_frameworks = target_->all_frameworks();
  for (size_t i = 0; i < all_frameworks.size(); i++) {
    writer(all_frameworks[i], out);
  }

  FrameworksWriter weak_writer(tool->weak_framework_switch());
  const auto& all_weak_frameworks = target_->all_weak_frameworks();
  for (size_t i = 0; i < all_weak_frameworks.size(); i++) {
    weak_writer(all_weak_frameworks[i], out);
  }
}

void NinjaBinaryTargetWriter::WriteSwiftModules(
    std::ostream& out,
    const Tool* tool,
    const std::vector<OutputFile>& swiftmodules) {
  // Since we're passing these on the command line to the linker and not
  // to Ninja, we need to do shell escaping.
  PathOutput swiftmodule_path_output(
      path_output_.current_dir(), settings_->build_settings()->root_path_utf8(),
      ESCAPE_NINJA_COMMAND);

  for (const OutputFile& swiftmodule : swiftmodules) {
    out << " " << tool->swiftmodule_switch();
    swiftmodule_path_output.WriteFile(out, swiftmodule);
  }
}
