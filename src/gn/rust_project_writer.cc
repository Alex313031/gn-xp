// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_project_writer.h"

#include <sstream>

#include "base/json/string_escape.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "gn/builder.h"
#include "gn/filesystem_utils.h"
#include "gn/ninja_target_command_util.h"
#include "gn/path_output.h"
#include "gn/substitution_writer.h"

#if defined(OS_WIN)
const char kPrettyPrintLineEnding[] = "\r\n";
#else
const char kPrettyPrintLineEnding[] = "\n";
#endif

bool RustProjectWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                         const Builder& builder,
                                         const std::string& file_name,
                                         bool quiet,
                                         Err* err) {
  SourceFile output_file = build_settings->build_dir().ResolveRelativeFile(
      Value(nullptr, file_name), err);
  if (output_file.is_null())
    return false;

  base::FilePath output_path = build_settings->GetFullPath(output_file);

  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();
  std::string json;

  RenderJSON(build_settings, all_targets, &json);
  if (!WriteFileIfChanged(output_path, json, err))
    return false;
  return true;
}

using TargetIdxMap = std::unordered_map<std::string, uint32_t>;

void WriteDeps(const Target* target,
               TargetIdxMap& lookup,
               std::string* rust_project_json) {
  rust_project_json->append("    \"deps\": [");
  rust_project_json->append(kPrettyPrintLineEnding);
  bool first = true;
  for (const auto& dep : target->rust_values().transitive_libs().GetOrdered()) {
    auto idx = lookup[dep->output_name()];
    if (!first) {
      rust_project_json->append(",");
      rust_project_json->append(kPrettyPrintLineEnding);
    } else {
      first = false;
    }

    rust_project_json->append("      {");
    rust_project_json->append(kPrettyPrintLineEnding);

    rust_project_json->append("        \"crate\": ");
    rust_project_json->append(std::to_string(idx));
    rust_project_json->append(",");
    rust_project_json->append(kPrettyPrintLineEnding);

    rust_project_json->append("        \"name\": \"");
    rust_project_json->append(dep->rust_values().crate_name());
    rust_project_json->append("\"");

    rust_project_json->append(kPrettyPrintLineEnding);
    rust_project_json->append("      }");
  }
  rust_project_json->append(kPrettyPrintLineEnding);
  rust_project_json->append("    ],");
  rust_project_json->append(kPrettyPrintLineEnding);
}

void AddTarget(const Target* target,
               uint32_t* count,
               TargetIdxMap& lookup,
               std::string* rust_project_json,
               bool first) {
  if (lookup.find(target->output_name()) != lookup.end()) {
    // if target is already in the lookup, we don't add it again
    return;
  }

  // add each dependency first before we write any of the parent target
  for (const auto& dep : target->rust_values().transitive_libs().GetOrdered()) {
    AddTarget(dep, count, lookup, rust_project_json, first);
    first = false;
  }

  // construct the crate info
  if (!first) {
    rust_project_json->append(",");
    rust_project_json->append(kPrettyPrintLineEnding);
  }
  rust_project_json->append("  {");
  rust_project_json->append(kPrettyPrintLineEnding);

  rust_project_json->append("    \"crate_id\": ");
  rust_project_json->append(std::to_string(*count));
  rust_project_json->append(",");
  rust_project_json->append(kPrettyPrintLineEnding);

  // add the target to the lookup
  lookup.insert(std::make_pair(target->output_name(), *count));
  (*count)++;

  rust_project_json->append("    \"root_module\": \"");
  rust_project_json->append(target->rust_values().crate_root().value());
  rust_project_json->append("\",");
  rust_project_json->append(kPrettyPrintLineEnding);

  WriteDeps(target, lookup, rust_project_json);

  rust_project_json->append("    \"atom_cfgs\": [],");
  rust_project_json->append(kPrettyPrintLineEnding);
  rust_project_json->append("    \"key_value_cfgs\": {},");
  rust_project_json->append(kPrettyPrintLineEnding);
  rust_project_json->append("    \"edition\": \"2018\"");
  rust_project_json->append(kPrettyPrintLineEnding);
  rust_project_json->append("  }");
}

void RustProjectWriter::RenderJSON(const BuildSettings* build_settings,
                                   std::vector<const Target*>& all_targets,
                                   std::string* rust_project_json) {
  // TODO: Determine out an appropriate size to reserve.
  rust_project_json->reserve(all_targets.size() * 100);

  TargetIdxMap lookup;
  uint32_t count = 0;
  bool first = true;

  rust_project_json->append("{");
  rust_project_json->append(kPrettyPrintLineEnding);

  rust_project_json->append("  \"crates\": [");
  rust_project_json->append(kPrettyPrintLineEnding);

  for (const auto* target : all_targets) {
    // TODO better way of identifying rust target
    if (!target->IsBinary() || target->rust_values().crate_name() == "")
      continue;

    AddTarget(target, &count, lookup, rust_project_json, first);
    first = false;
  }

  // close crates array
  rust_project_json->append("  ]");
  rust_project_json->append(kPrettyPrintLineEnding);

  // close global object
  rust_project_json->append("}");
  rust_project_json->append(kPrettyPrintLineEnding);
}
