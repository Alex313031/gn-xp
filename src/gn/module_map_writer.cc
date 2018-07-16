// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/module_map_writer.h"

#include <sstream>

#include "base/files/file_util.h"
#include "gn/builder.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/loader.h"
#include "gn/location.h"
#include "gn/settings.h"
#include "gn/source_file.h"
#include "gn/target.h"

ModuleMapWriter::ModuleMapWriter(const Target* target,
                                 std::ostream& out,
                                 bool build_dir_relative)
    : settings_(target->settings()),
      target_(target),
      out_(out),
      path_output_(
          build_dir_relative
              ? settings_->build_settings()->build_dir()
              : GetBuildDirForTargetAsSourceDir(target, BuildDirType::OBJ),
          settings_->build_settings()->root_path_utf8(),
          ESCAPE_NONE) {}

// TODO: move to a different file?
SourceFile GetCppMapFileForTarget(const Target* target) {
  if (target->module_map().is_null())
    return SourceFile(
        GetBuildDirForTargetAsSourceDir(target, BuildDirType::OBJ).value() +
        target->label().name() + ".cppmap");
  else
    return target->module_map();
}

std::string GetModuleNameForTarget(const Target* target) {
  if (!target->module_name().empty())
    return target->module_name();
  else
    return target->label().GetUserVisibleName(false);
}

// static
SourceFile ModuleMapWriter::RunAndWriteFile(const Target* target) {
  DCHECK(target->module_map().is_null());

  const Settings* settings = target->settings();

  std::stringstream rules;

  SourceFile module_map_file = GetCppMapFileForTarget(target);
  base::FilePath full_module_map_file =
      settings->build_settings()->GetFullPath(module_map_file);

  ModuleMapWriter writer(target, rules);
  writer.Run();

  base::CreateDirectory(full_module_map_file.DirName());
  WriteFileIfChanged(full_module_map_file, rules.str(), nullptr);

  return module_map_file;
}

void ModuleMapWriter::WriteHeader(const SourceFile& source, bool privat) {
  out_ << "  ";
  if (generate_submodules_) {
    out_ << "module \"" << source.GetName() << "\" {\n";
    out_ << "    export *\n    ";
  }
  if (privat)
    out_ << "private ";
  out_ << "header \"";
  path_output_.WriteFile(out_, source);
  out_ << "\"\n";
  if (generate_submodules_)
    out_ << "  }\n";
}

void ModuleMapWriter::Run() {
  Label default_toolchain_label;

  if (default_toolchain_label.is_null())
    default_toolchain_label = target_->settings()->default_toolchain_label();

  out_ << "module \"" << GetModuleNameForTarget(target_) << "\" {\n";
  out_ << "  export *\n";

  bool default_public = target_->all_headers_public();
  if (default_public)  // List only used when default is not public.
    DCHECK(target_->public_headers().empty());
  for (const auto& source : target_->public_headers()) {
    WriteHeader(source, false);
  }

  for (const auto& source : target_->sources()) {
    if (GetSourceFileType(source.value()) == SourceFile::SOURCE_H) {
      WriteHeader(source, !default_public);
    }
  }

  for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED)) {
    out_ << "  use \"" << GetModuleNameForTarget(pair.ptr) << "\"\n";
  }

  out_ << "}";

  if (extern_dependencies_) {
    for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED)) {
      SourceFile module_map_file = GetCppMapFileForTarget(pair.ptr);
      out_ << "\nextern module \"" << GetModuleNameForTarget(pair.ptr)
           << "\" \"";
      path_output_.WriteFile(out_, module_map_file);
      out_ << "\"";
    }
  }

  out_ << "\n";
}
