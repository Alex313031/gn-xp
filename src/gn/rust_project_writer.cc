// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/rust_project_writer.h"

#include <fstream>
#include <sstream>
#include <tuple>

#include "base/json/string_escape.h"
#include "gn/builder.h"
#include "gn/filesystem_utils.h"
#include "gn/ninja_target_command_util.h"

#if defined(OS_WINDOWS)
#define LINE_ENDING "\r\n"
#else
#define LINE_ENDING "\n"
#endif

// Current structure of rust-project.json output file
//
// {
//    "roots": [] // always empty for GN. To be deprecated.
//    "crates": [
//        {
//            "atom_cfgs": [], // atom config options
//            "deps": [
//                {
//                    "crate": 1, // index into crate array
//                    "name": "alloc" // extern name of dependency
//                },
//            ],
//            "edition": "2018", // edition of crate
//            "key_value_cfgs": {
//              "rust_panic": "abort" // key value config options
//            },
//            "root_module": "absolute path to crate"
//        },
// }
//

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

  std::ofstream json;
  json.open(FilePathToUTF8(output_path).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (json.fail())
    return false;

  RenderJSON(build_settings, all_targets, json);

  return true;
}

using TargetIdxMap = std::unordered_map<std::string, uint32_t>;

void WriteDeps(const Target* target,
               TargetIdxMap& lookup,
               std::ostream& rust_project) {
  rust_project << "    \"deps\": [" << LINE_ENDING;
  bool first = true;
  for (const auto& dep : target->rust_values().transitive_libs().GetOrdered()) {
    auto idx = lookup[dep->output_name()];
    if (!first) {
      rust_project << "," << LINE_ENDING;
    }
    first = false;
    rust_project << "      {" << LINE_ENDING;
    rust_project << "        \"crate\": " << std::to_string(idx) << ","
                 << LINE_ENDING;
    rust_project << "        \"name\": \"" << dep->rust_values().crate_name()
                 << "\"" << LINE_ENDING;
    rust_project << "      }";
  }
  rust_project << LINE_ENDING << "    ]," << LINE_ENDING;
}

void AddTarget(const Target* target,
               uint32_t* count,
               TargetIdxMap& lookup,
               const BuildSettings* build_settings,
               std::ostream& rust_project,
               bool first) {
  if (lookup.find(target->output_name()) != lookup.end()) {
    // if target is already in the lookup, we don't add it again
    return;
  }

  // add each dependency first before we write any of the parent target
  for (const auto& dep : target->rust_values().transitive_libs().GetOrdered()) {
    AddTarget(dep, count, lookup, build_settings, rust_project, first);
    first = false;
  }

  // construct the crate info
  if (!first) {
    rust_project << "," << LINE_ENDING;
  }
  rust_project << "  {" << LINE_ENDING;
  rust_project << "    \"crate_id\": " << std::to_string(*count) << ","
               << LINE_ENDING;

  // add the target to the lookup
  lookup.insert(std::make_pair(target->output_name(), *count));
  (*count)++;

  base::FilePath crate_root =
      build_settings->GetFullPath(target->rust_values().crate_root());

  rust_project << "    \"root_module\": \"" << FilePathToUTF8(crate_root)
               << "\"," << LINE_ENDING;

  WriteDeps(target, lookup, rust_project);

  std::string cfg_prefix("--cfg=");
  std::string edition_prefix("--edition=");
  std::vector<std::string> atoms;
  std::vector<std::tuple<std::string, std::string>> kvs;

  for (ConfigValuesIterator iter(target); !iter.done(); iter.Next()) {
    for (const auto& flag : iter.cur().rustflags()) {
      // extract the edition of this target
      if (!flag.compare(0, edition_prefix.size(), edition_prefix)) {
        auto edition = flag.substr(edition_prefix.size());
        rust_project << "    \"edition\": \"" << edition << "\","
                     << LINE_ENDING;
      }
      // we can't directly print cfgs since they come in any order
      // if they have an = they are a k/v cfg, otherwise an atom cfg
      if (!flag.compare(0, cfg_prefix.size(), cfg_prefix)) {
        auto cfg = flag.substr(cfg_prefix.size());
        auto idx = cfg.rfind("=");
        if (idx == std::string::npos) {
          atoms.push_back(cfg);
        } else {
          std::string key = cfg.substr(0, idx);
          std::string value = cfg.substr(idx + 1);
          kvs.push_back(std::make_pair(key, value));
        }
      }
    }
  }

  rust_project << "    \"atom_cfgs\": [";
  bool first_atom = true;
  for (const auto& cfg : atoms) {
    if (!first_atom) {
      rust_project << ",";
    }
    first_atom = false;
    rust_project << LINE_ENDING << "      \"" << cfg << "\"";
  }
  rust_project << LINE_ENDING << "     ]," << LINE_ENDING;

  rust_project << "    \"key_value_cfgs\": {";
  bool first_kv = true;
  for (const auto cfg : kvs) {
    if (!first_kv) {
      rust_project << ",";
    }
    first_kv = false;
    rust_project << LINE_ENDING << "      \"" << std::get<0>(cfg)
                 << "\" : " << std::get<1>(cfg);
  }
  rust_project << LINE_ENDING << "    }";
  rust_project << LINE_ENDING << "  }";
}

void RustProjectWriter::RenderJSON(const BuildSettings* build_settings,
                                   std::vector<const Target*>& all_targets,
                                   std::ostream& rust_project) {
  TargetIdxMap lookup;
  uint32_t count = 0;
  bool first = true;

  rust_project << "{" << LINE_ENDING;

  rust_project << "\"roots\": []," << LINE_ENDING;
  rust_project << "\"crates\": [," << LINE_ENDING;

  for (const auto* target : all_targets) {
    // TODO better way of identifying rust target
    if (!target->IsBinary() || target->rust_values().crate_name() == "")
      continue;

    AddTarget(target, &count, lookup, build_settings, rust_project, first);
    first = false;
  }

  // close crates array
  rust_project << "  ]" << LINE_ENDING;
  // close global object
  rust_project << "}" << LINE_ENDING;
}
