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
#include "gn/rust_tool.h"
#include "gn/source_file.h"
#include "gn/tool.h"

#if defined(OS_WINDOWS)
#define NEWLINE "\r\n"
#else
#define NEWLINE "\n"
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

using TargetIdxMap = std::unordered_map<const Target*, uint32_t>;
using SysrootIdxMap = std::unordered_map<std::string, uint32_t>;

void WriteDeps(const Target* target,
               TargetIdxMap& lookup,
               SysrootIdxMap& sysroot_lookup,
               std::ostream& rust_project) {
  rust_project << "    \"deps\": [" NEWLINE;

  // TODO(bwb) If this library doesn't depend on std, use core instead
  auto std_idx = sysroot_lookup["std"];
  rust_project << "      {" NEWLINE
               << "        \"crate\": " << std::to_string(std_idx)
               << "," NEWLINE << "        \"name\": \"std\"" NEWLINE
               << "      }";

  for (const auto& dep : target->rust_values().transitive_libs().GetOrdered()) {
    auto idx = lookup[dep];
    rust_project << "," NEWLINE;
    rust_project << "      {" NEWLINE
                 << "        \"crate\": " << std::to_string(idx) << "," NEWLINE
                 << "        \"name\": \"" << dep->rust_values().crate_name()
                 << "\"" NEWLINE << "      }";
  }
  rust_project << NEWLINE "    ]," NEWLINE;
}

const std::string sysroot_edition = "2018";
const std::string sysroot_crates[23] = {"std",
                                        "core",
                                        "alloc",
                                        "collections",
                                        "libc",
                                        "panic_unwind",
                                        "proc_macro",
                                        "rustc_unicode",
                                        "std_unicode",
                                        "test",
                                        "alloc_jemalloc",
                                        "alloc_system",
                                        "compiler_builtins",
                                        "getopts",
                                        "panic_unwind",
                                        "panic_abort",
                                        "unwind",
                                        "build_helper",
                                        "rustc_asan",
                                        "rustc_lsan",
                                        "rustc_msan",
                                        "rustc_tsan",
                                        "syntax"};

const std::string std_deps[4] = {
    "alloc",
    "core",
    "panic_abort",
    "unwind",

};

void AddSysrootCrate(std::string crate,
                     base::FilePath rustc_path,
                     uint32_t* count,
                     SysrootIdxMap& sysroot_lookup,
                     std::ostream& rust_project,
                     bool first) {
  // fake std's deps
  if (crate == "std") {
    for (auto dep : std_deps) {
      AddSysrootCrate(dep, rustc_path, count, sysroot_lookup, rust_project,
                      first);
      first = false;
    }
  }

  if (!first) {
    rust_project << ",";
  }

  sysroot_lookup.insert(std::make_pair(crate, *count));
  (*count)++;

  auto crate_path = FilePathToUTF8(rustc_path) +
                    "/../lib/rustlib/src/rust/src/lib" + crate + "/lib.rs";
  rust_project << NEWLINE "  {" NEWLINE;
  rust_project << "    \"crate_id\": " << std::to_string(*count) << "," NEWLINE;
  rust_project << "    \"root_module\": \"" << crate_path << "\"," NEWLINE;
  rust_project << "    \"edition\": \"2018\"," NEWLINE;
  rust_project << "    \"deps\": [" NEWLINE;
  if (crate == "std") {
    first = true;
    for (auto dep : std_deps) {
      auto idx = sysroot_lookup[dep];
      if (!first) {
        rust_project << "," NEWLINE;
      }
      first = false;
      rust_project << "      {" NEWLINE
                   << "        \"crate\": " << std::to_string(idx)
                   << "," NEWLINE << "        \"name\": \"" << dep
                   << "\"" NEWLINE << "      }";
    }
  }
  rust_project << NEWLINE "    ]," NEWLINE;
  rust_project << "    \"atom_cfgs\": []," NEWLINE
                  "    \"key_value_cfgs\": {}" NEWLINE "  }";
}

void AddTarget(const Target* target,
               uint32_t* count,
               TargetIdxMap& lookup,
               SysrootIdxMap& sysroot_lookup,
               const BuildSettings* build_settings,
               std::ostream& rust_project,
               bool first) {
  if (lookup.find(target) != lookup.end()) {
    // if target is already in the lookup, we don't add it again
    return;
  }

  // add each dependency first before we write any of the parent target
  for (const auto& dep : target->rust_values().transitive_libs().GetOrdered()) {
    AddTarget(dep, count, lookup, sysroot_lookup, build_settings, rust_project,
              first);
    first = false;
  }

  if (!first) {
    rust_project << "," NEWLINE;
  }

  // construct the crate info
  rust_project << "  {" NEWLINE;
  rust_project << "    \"crate_id\": " << std::to_string(*count) << "," NEWLINE;

  // add the target to the lookup
  lookup.insert(std::make_pair(target, *count));
  (*count)++;

  base::FilePath crate_root =
      build_settings->GetFullPath(target->rust_values().crate_root());

  rust_project << "    \"root_module\": \"" << FilePathToUTF8(crate_root)
               << "\"," NEWLINE;

  WriteDeps(target, lookup, sysroot_lookup, rust_project);

  std::string cfg_prefix("--cfg=");
  std::string edition_prefix("--edition=");
  std::vector<std::string> atoms;
  std::vector<std::tuple<std::string, std::string>> kvs;

  for (ConfigValuesIterator iter(target); !iter.done(); iter.Next()) {
    for (const auto& flag : iter.cur().rustflags()) {
      // extract the edition of this target
      if (!flag.compare(0, edition_prefix.size(), edition_prefix)) {
        auto edition = flag.substr(edition_prefix.size());
        rust_project << "    \"edition\": \"" << edition << "\"," NEWLINE;
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
    rust_project << NEWLINE "      \"" << cfg << "\"";
  }
  rust_project << NEWLINE "     ]," NEWLINE;

  rust_project << "    \"key_value_cfgs\": {";
  bool first_kv = true;
  for (const auto cfg : kvs) {
    if (!first_kv) {
      rust_project << ",";
    }
    first_kv = false;
    rust_project << NEWLINE "      \"" << std::get<0>(cfg)
                 << "\" : " << std::get<1>(cfg);
  }
  rust_project << NEWLINE "    }";
  rust_project << NEWLINE "  }";
}

void RustProjectWriter::RenderJSON(const BuildSettings* build_settings,
                                   std::vector<const Target*>& all_targets,
                                   std::ostream& rust_project) {
  TargetIdxMap lookup;
  SysrootIdxMap sysroot_lookup;
  uint32_t count = 0;
  bool first = true;

  rust_project << "{" NEWLINE;

  rust_project << "\"roots\": []," NEWLINE;
  rust_project << "\"crates\": [" NEWLINE;

  // Sysroot crates
  auto rustc_cmd = all_targets[0]
                       ->toolchain()
                       ->GetToolForSourceTypeAsRust(SourceFile::SOURCE_RS)
                       ->command()
                       .AsString();
  std::istringstream iss(rustc_cmd);
  std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                                  std::istream_iterator<std::string>{}};

  for (auto cmd : tokens) {
    auto idx = cmd.rfind("bin/rustc");
    if (idx != std::string::npos) {
      SourceFile out = build_settings->build_dir().ResolveRelativeFile(
          Value(nullptr, cmd), nullptr);
      base::FilePath rustc_path = build_settings->GetFullPath(out).DirName();
      for (const auto& crate : sysroot_crates) {
        AddSysrootCrate(crate, rustc_path, &count, sysroot_lookup, rust_project,
                        first);
        first = false;
      }
      break;
    }
  }

  // All the crates defined in the project
  for (const auto* target : all_targets) {
    // TODO better way of identifying rust target
    if (!target->IsBinary() || target->rust_values().crate_name() == "")
      continue;

    AddTarget(target, &count, lookup, sysroot_lookup, build_settings,
              rust_project, first);
    first = false;
  }

  // close crates array
  rust_project << "  ]" NEWLINE;
  // close global object
  rust_project << "}" NEWLINE;
}
