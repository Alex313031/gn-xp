// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mutex>
#include <thread>
#include <unordered_map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/timer/elapsed_timer.h"
#include "gn/build_settings.h"
#include "gn/commands.h"
#include "gn/compile_commands_writer.h"
#include "gn/eclipse_writer.h"
#include "gn/filesystem_utils.h"
#include "gn/gen.h"
#include "gn/json_project_writer.h"
#include "gn/label_pattern.h"
#include "gn/ninja_target_writer.h"
#include "gn/ninja_tools.h"
#include "gn/ninja_writer.h"
#include "gn/qt_creator_writer.h"
#include "gn/runtime_deps.h"
#include "gn/rust_project_writer.h"
#include "gn/scheduler.h"
#include "gn/setup.h"
#include "gn/standard_out.h"
#include "gn/switches.h"
#include "gn/target.h"
#include "gn/visual_studio_writer.h"
#include "gn/xcode_writer.h"

namespace commands {

const char kGen[] = "gen";
const char kGen_HelpShort[] = "gen: Generate ninja files.";
const char kGen_Help[] =
    R"(gn gen [--check] [<ide options>] <out_dir>

  Generates ninja files from the current tree and puts them in the given output
  directory.

  The output directory can be a source-repo-absolute path name such as:
      //out/foo
  Or it can be a directory relative to the current directory such as:
      out/foo

  "gn gen --check" is the same as running "gn check". "gn gen --check=system" is
  the same as running "gn check --check-system".  See "gn help check" for
  documentation on that mode.

  See "gn help switches" for the common command-line switches.

General options

  --ninja-executable=<string>
      Can be used to specify the ninja executable to use. This executable will
      be used as an IDE option to indicate which ninja to use for building. This
      executable will also be used as part of the gen process for triggering a
      restat on generated ninja files and for use with --clean-stale.

  --clean-stale
      This option will cause no longer needed output files to be removed from
      the build directory, and their records pruned from the ninja build log and
      dependency database after the ninja build graph has been generated. This
      option requires a ninja executable of at least version 1.10.0. It can be
      provided by the --ninja-executable switch. Also see "gn help clean_stale".

IDE options

  GN optionally generates files for IDE. Files won't be overwritten if their
  contents don't change. Possibilities for <ide options>

  --ide=<ide_name>
      Generate files for an IDE. Currently supported values:
      "eclipse" - Eclipse CDT settings file.
      "vs" - Visual Studio project/solution files.
             (default Visual Studio version: 2019)
      "vs2013" - Visual Studio 2013 project/solution files.
      "vs2015" - Visual Studio 2015 project/solution files.
      "vs2017" - Visual Studio 2017 project/solution files.
      "vs2019" - Visual Studio 2019 project/solution files.
      "vs2022" - Visual Studio 2022 project/solution files.
      "xcode" - Xcode workspace/solution files.
      "qtcreator" - QtCreator project files.
      "json" - JSON file containing target information

  --filters=<path_prefixes>
      Semicolon-separated list of label patterns used to limit the set of
      generated projects (see "gn help label_pattern"). Only matching targets
      and their dependencies will be included in the solution. Only used for
      Visual Studio, Xcode and JSON.

Visual Studio Flags

  --sln=<file_name>
      Override default sln file name ("all"). Solution file is written to the
      root build directory.

  --no-deps
      Don't include targets dependencies to the solution. Changes the way how
      --filters option works. Only directly matching targets are included.

  --winsdk=<sdk_version>
      Use the specified Windows 10 SDK version to generate project files.
      As an example, "10.0.15063.0" can be specified to use Creators Update SDK
      instead of the default one.

  --ninja-executable=<string>
      Can be used to specify the ninja executable to use when building.

  --ninja-extra-args=<string>
      This string is passed without any quoting to the ninja invocation
      command-line. Can be used to configure ninja flags, like "-j".

Xcode Flags

  --xcode-project=<file_name>
      Override default Xcode project file name ("all"). The project file is
      written to the root build directory.

  --xcode-build-system=<value>
      Configure the build system to use for the Xcode project. Supported
      values are (default to "legacy"):
      "legacy" - Legacy Build system
      "new" - New Build System

  --xcode-configs=<config_name_list>
      Configure the list of build configuration supported by the generated
      project. If specified, must be a list of semicolon-separated strings.
      If ommitted, a single configuration will be used in the generated
      project derived from the build directory.

  --xcode-config-build-dir=<string>
      If present, must be a path relative to the source directory. It will
      default to $root_out_dir if ommitted. The path is assumed to point to
      the directory where ninja needs to be invoked. This variable can be
      used to build for multiple configuration / platform / environment from
      the same generated Xcode project (assuming that the user has created a
      gn build directory with the correct args.gn for each).

      One useful value is to use Xcode variables such as '${CONFIGURATION}'
      or '${EFFECTIVE_PLATFORM}'.

  --xcode-additional-files-patterns=<pattern_list>
      If present, must be a list of semicolon-separated file patterns. It
      will be used to add all files matching the pattern located in the
      source tree to the project. It can be used to add, e.g. documentation
      files to the project to allow easily edit them.

  --xcode-additional-files-roots=<path_list>
      If present, must be a list of semicolon-separated paths. It will be used
      as roots when looking for additional files to add. If ommitted, defaults
      to "//".

  --ninja-executable=<string>
      Can be used to specify the ninja executable to use when building.

  --ninja-extra-args=<string>
      This string is passed without any quoting to the ninja invocation
      command-line. Can be used to configure ninja flags, like "-j".

  --ide-root-target=<target_name>
      Name of the target corresponding to "All" target in Xcode. If unset,
      "All" invokes ninja without any target and builds everything.

QtCreator Flags

  --ide-root-target=<target_name>
      Name of the root target for which the QtCreator project will be generated
      to contain files of it and its dependencies. If unset, the whole build
      graph will be emitted.


Eclipse IDE Support

  GN DOES NOT generate Eclipse CDT projects. Instead, it generates a settings
  file which can be imported into an Eclipse CDT project. The XML file contains
  a list of include paths and defines. Because GN does not generate a full
  .cproject definition, it is not possible to properly define includes/defines
  for each file individually. Instead, one set of includes/defines is generated
  for the entire project. This works fairly well but may still result in a few
  indexer issues here and there.

Generic JSON Output

  Dumps target information to a JSON file and optionally invokes a
  python script on the generated file. See the comments at the beginning
  of json_project_writer.cc and desc_builder.cc for an overview of the JSON
  file format.

  --json-file-name=<json_file_name>
      Overrides default file name (project.json) of generated JSON file.

  --json-ide-script=<path_to_python_script>
      Executes python script after the JSON file is generated or updated with
      new content. Path can be project absolute (//), system absolute (/) or
      relative, in which case the output directory will be base. Path to
      generated JSON file will be first argument when invoking script.

  --json-ide-script-args=<argument>
      Optional second argument that will passed to executed script.

Compilation Database

  --export-rust-project
      Produces a rust-project.json file in the root of the build directory
      This is used for various tools in the Rust ecosystem allowing for the
      replay of individual compilations independent of the build system.
      This is an unstable format and likely to change without warning.

  --add-export-compile-commands=<label_pattern>
      Adds an additional label pattern (see "gn help label_pattern") of a
      target to add to the compilation database. This pattern is appended to any
      list values specified in the export_compile_commands variable in the
      .gn file (see "gn help dotfile"). This allows the user to add additional
      targets to the compilation database that the project doesn't add by default.

      To add more than one value, specify this switch more than once. Each
      invocation adds an additional label pattern.

      Example:
        --add-export-compile-commands=//tools:my_tool
        --add-export-compile-commands="//base/*"

  --export-compile-commands[=<target_name1,target_name2...>]
      DEPRECATED https://bugs.chromium.org/p/gn/issues/detail?id=302.
      Please use --add-export-compile-commands for per-user configuration, and
      the "export_compile_commands" value in the project-level .gn file (see
      "gn help dotfile") for per-project configuration.

      Overrides the value of the export_compile_commands in the .gn file (see
      "gn help dotfile") as well as the --add-export-compile-commands switch.

      Unlike the .gn setting, this switch takes a legacy format which is a list
      of target names that are matched in any directory. For example, "foo" will
      match:
       - "//path/to/src:foo"
       - "//other/path:foo"
       - "//foo:foo"
      and not match:
       - "//foo:bar"
)";

int RunGen(const std::vector<std::string>& args) {
  return Gen::Run(args);
}

}  // namespace commands
