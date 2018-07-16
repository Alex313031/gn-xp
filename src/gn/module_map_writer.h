// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_MODULE_MAP_H_
#define TOOLS_GN_MODULE_MAP_H_

#include <iosfwd>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "gn/path_output.h"

class Err;
class Settings;
class Target;
class SourceFile;

class ModuleMapWriter {
 public:
  // Creates a new module.modulemap file writer for a given target.
  //
  // Note that while |build_dir_relative| generates cleaner files, its use
  // requires passing -Xclang -fmodule-map-file-home-is-cwd flag to Clang
  // when compiling the map which is why it's not currently the default.
  ModuleMapWriter(const Target* target,
                  std::ostream& out,
                  bool build_dir_relative = false);
  ~ModuleMapWriter() = default;

  static SourceFile RunAndWriteFile(const Target* target);

  void Run();

  bool extern_dependencies() const { return extern_dependencies_; }
  void set_extern_dependencies(bool v) { extern_dependencies_ = v; }

  bool generate_submodules() const { return generate_submodules_; }
  void set_generate_submodules(bool v) { generate_submodules_ = v; }

 private:
  void WriteHeader(const SourceFile& source, bool privat);

  const Settings* settings_;
  const Target* target_;
  std::ostream& out_;
  PathOutput path_output_;

  bool extern_dependencies_ = true;
  bool generate_submodules_ = true;

  DISALLOW_COPY_AND_ASSIGN(ModuleMapWriter);
};

SourceFile GetCppMapFileForTarget(const Target* target);
std::string GetModuleNameForTarget(const Target* target);

#endif  // TOOLS_GN_MODULE_MAP_WRITER_H_
