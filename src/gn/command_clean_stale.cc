// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "gn/commands.h"
#include "gn/err.h"
#include "gn/ninja_tools.h"
#include "gn/switches.h"
#include "gn/version.h"

namespace commands {

namespace {

bool CleanStaleOneDir(const base::FilePath& ninja_executable,
                      const std::string& dir) {
  std::optional<Version> ninja_version = GetNinjaVersion(ninja_executable);
  if (!ninja_version) {
    Err(Location(), "Could not determine ninja version.",
        "clean_stale requires a ninja executable to run. You can either\n"
        "provide one on the command line via --ninja-executable or by having\n"
        "the executable in your path.")
        .PrintToStdout();
    return false;
  }

  if (*ninja_version < Version{1, 10, 0}) {
    Err(Location(), "Need a ninja executable at least version 1.10.0",
        "clean_stale requires a ninja executable of version 1.10.0 or later.")
        .PrintToStdout();
    return false;
  }

  // The ideal order of operations for these tools is:
  // 1. cleandead - This eliminates old files from the build directory.
  // 2. recompact - This compacts the ninja log and deps files.
  base::FilePath dir_path{dir};
  Err err;
  if (!InvokeNinjaCleanDeadTool(ninja_executable, dir_path, &err)) {
    err.PrintToStdout();
    return false;
  }

  if (!InvokeNinjaRecompactTool(ninja_executable, dir_path, &err)) {
    return false;
  }
  return true;
}

}

const char kCleanStale[] = "clean_stale";
const char kCleanStale_HelpShort[] =
    "clean_stale: Cleans the stale output files from the output directory.";
const char kCleanStale_Help[] =
    R"(gn clean_stale [--ninja-executable=...] <out_dir>...

  Removes the no longer needed output files from the build directory and prunes
  their records from the ninja build log and dependency database. These are
  output files that were generated from previous builds, but the current build
  graph no longer references them.

  This command requires a ninja executable of at least version 1.10.0. The
  executable can be provided by the --ninja-executable switch or exist on the
  path.

Options

  --ninja-executable=<string>
      Can be used to specify the ninja executable to use.
)";

int RunCleanStale(const std::vector<std::string>& args) {
  if (args.empty()) {
    Err(Location(), "Missing argument.",
        "Usage: \"gn clean_stale <out_dir>...\"")
        .PrintToStdout();
    return 1;
  }

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  base::FilePath ninja_executable =
      cmdline->HasSwitch(switches::kNinjaExecutable)
          ? cmdline->GetSwitchValuePath(switches::kNinjaExecutable)
          : base::FilePath{"ninja"};

  for (const auto& dir : args) {
    if (!CleanStaleOneDir(ninja_executable, dir))
      return 1;
  }

  return 0;
}

}  // namespace commands
