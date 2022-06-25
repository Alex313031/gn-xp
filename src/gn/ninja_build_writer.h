// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BUILD_WRITER_H_
#define TOOLS_GN_NINJA_BUILD_WRITER_H_

#include <iosfwd>
#include <map>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "gn/path_output.h"

class Builder;
class BuildSettings;
class Err;
class Settings;
class Target;
class Toolchain;

namespace base {
class CommandLine;
}  // namespace base

// Generates the toplevel "build.ninja" file. This references the individual
// toolchain files and lists all input .gn files as dependencies of the
// build itself.
class NinjaBuildWriter {
 public:
  NinjaBuildWriter(const BuildSettings* settings,
                   const std::unordered_map<const Settings*, const Toolchain*>&
                       used_toolchains,
                   const std::vector<const Target*>& all_targets,
                   const Toolchain* default_toolchain,
                   const Settings* default_toolchain_settings,
                   const std::vector<const Target*>& default_toolchain_targets,
                   std::ostream& out,
                   std::ostream& dep_out);
  ~NinjaBuildWriter();

  // The design of this class is that this static factory function takes the
  // Builder, extracts the relevant information, and passes it to the class
  // constructor. The class itself doesn't depend on the Builder at all which
  // makes testing much easier (tests integrating various functions along with
  // the Builder get very complicated).
  //
  // If is_regeneration is false, the ninja file contents are written first to
  // "build.ninja.tmp" and its depfile to "build.ninja.d", and then
  // "build.ninja.tmp" is copied to "build.ninja". If is_regeneration is true,
  // the copy is skipped since it will be performed by ninja.
  static bool RunAndWriteFile(const BuildSettings* settings,
                              const Builder& builder,
                              bool is_regeneration,
                              Err* err);

  // Extracts from an existing build.ninja file's contents the commands
  // necessary to run GN and regenerate build.ninja.
  //
  // The regeneration rules live at the top of the build.ninja file and their
  // specific contents are an internal detail of NinjaBuildWriter. Used by
  // commands::PrepareForRegeneration.
  //
  // On error, returns an empty string.
  static std::string ExtractRegenerationCommands(
      const std::string& build_ninja_in);

  bool Run(Err* err);

 private:
  // WriteNinjaRules writes the rules that ninja uses to regenerate its own
  // build files, used whenever a build input file has changed.
  //
  // Ninja file regeneration is accomplished by two separate build statements.
  // The first runs the gen command with the "--regeneration" switch, trimming
  // the existing "build.ninja" down to just these rules and then producing a
  // "build.ninja.tmp" file. The first step also lists the "build.ninja.d"
  // depfile to capture implicit dependencies. The second simply copies the
  // "build.ninja.tmp" to "build.ninja".
  //
  // This careful dance is necessary to guarantee that the main "build.ninja"
  // will not be deleted by ninja if regeneration is interrupted, which ninja
  // would otherwise do due to the depfile usage. This in turn ensures that
  // ninja still has the rules needed to regenerate without requiring the user
  // to manually invoke the gen command again. It also ensures that any build
  // settings which are captured only in the regeneration command line will not
  // be lost if regeneration is interrupted.
  void WriteNinjaRules();
  void WriteAllPools();
  bool WriteSubninjas(Err* err);
  bool WritePhonyAndAllRules(Err* err);

  void WritePhonyRule(const Target* target, std::string_view phony_name);

  const BuildSettings* build_settings_;

  const std::unordered_map<const Settings*, const Toolchain*>& used_toolchains_;
  const std::vector<const Target*>& all_targets_;
  const Toolchain* default_toolchain_;
  const Settings* default_toolchain_settings_;
  const std::vector<const Target*>& default_toolchain_targets_;

  std::ostream& out_;
  std::ostream& dep_out_;
  PathOutput path_output_;

  NinjaBuildWriter(const NinjaBuildWriter&) = delete;
  NinjaBuildWriter& operator=(const NinjaBuildWriter&) = delete;
};

extern const char kNinjaRules_Help[];

// Exposed for testing.
base::CommandLine GetSelfInvocationCommandLine(
    const BuildSettings* build_settings);

#endif  // TOOLS_GN_NINJA_BUILD_WRITER_H_
