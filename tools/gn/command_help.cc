// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "tools/gn/args.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/label.h"
#include "tools/gn/label_pattern.h"
#include "tools/gn/ninja_build_writer.h"
#include "tools/gn/output_conversion.h"
#include "tools/gn/parser.h"
#include "tools/gn/runtime_deps.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"
#include "tools/gn/variables.h"

namespace commands {

namespace {

void PrintToplevelHelp() {
  PrintSectionHelp("Commands", "<command>", "commands");
  for (const auto& cmd : commands::GetCommands())
    PrintShortHelp(cmd.second.help_short);

  // Target declarations.
  PrintSectionHelp("Target declarations", "<function>", "targets");
  for (const auto& func : functions::GetFunctions()) {
    if (func.second.is_target)
      PrintShortHelp(func.second.help_short);
  }

  // Functions.
  PrintSectionHelp("Buildfile functions", "<function>", "functions");
  for (const auto& func : functions::GetFunctions()) {
    if (!func.second.is_target)
      PrintShortHelp(func.second.help_short);
  }

  // Built-in variables.
  PrintSectionHelp("Built-in predefined variables", "<variable>",
                   "predefined_variables");
  for (const auto& builtin : variables::GetBuiltinVariables())
    PrintShortHelp(builtin.second.help_short);

  // Target variables.
  PrintSectionHelp("Variables you set in targets", "<variable>",
                   "target_variables");
  for (const auto& target : variables::GetTargetVariables())
    PrintShortHelp(target.second.help_short);

  PrintSectionHelp("Other help topics", "", "other");
  PrintShortHelp("all: Print all the help at once");
  PrintShortHelp("buildargs: How build arguments work.");
  PrintShortHelp("dotfile: Info about the toplevel .gn file.");
  PrintShortHelp("execution: Build graph and execution overview.");
  PrintShortHelp("grammar: Language and grammar for GN build files.");
  PrintShortHelp(
      "input_conversion: Processing input from exec_script and read_file.");
  PrintShortHelp("label_pattern: Matching more than one label.");
  PrintShortHelp("labels: About labels.");
  PrintShortHelp("ninja_rules: How Ninja build rules are named.");
  PrintShortHelp("nogncheck: Annotating includes for checking.");
  PrintShortHelp(
      "output_conversion: Specifies how to transform a value to output.");
  PrintShortHelp("runtime_deps: How runtime dependency computation works.");
  PrintShortHelp("source_expansion: Map sources to outputs for scripts.");
  PrintShortHelp("switches: Show available command-line switches.");
}

void PrintSwitchHelp() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool is_markdown = cmdline->HasSwitch(switches::kMarkdown);

  OutputString("Available global switches\n", DECORATION_YELLOW);
  OutputString(
      "  Do \"gn help --the_switch_you_want_help_on\" for more. Individual\n"
      "  commands may take command-specific switches not listed here. See the\n"
      "  help on your specific command for more.\n\n");

  if (is_markdown)
    OutputString("```\n", DECORATION_NONE);

  for (const auto& s : switches::GetSwitches())
    PrintShortHelp(s.second.short_help);

  if (is_markdown)
    OutputString("```\n", DECORATION_NONE);

  OutputString("\n");
}

void PrintAllHelp() {
  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool is_markdown = cmdline->HasSwitch(switches::kMarkdown);

  if (is_markdown) {
    OutputString("# GN Reference\n\n");
    OutputString(
        "*This page is automatically generated from* "
        "`gn help --markdown all`.\n\n");

    // Generate our own table of contents so that we have more control
    // over what's in and out.
    OutputString("## Contents\n\n");
  }

  PrintToplevelHelp();

  OutputString("\n");

  if (is_markdown)
    OutputString("## <a name=\"commands\"></a>Commands\n\n");
  for (const auto& c : commands::GetCommands())
    PrintLongHelp(c.second.help);

  if (is_markdown)
    OutputString("## <a name=\"targets\"></a>Target declarations\n\n");
  for (const auto& f : functions::GetFunctions()) {
    if (f.second.is_target)
      PrintLongHelp(f.second.help);
  }

  if (is_markdown)
    OutputString("## <a name=\"functions\"></a>Buildfile functions\n\n");
  for (const auto& f : functions::GetFunctions()) {
    if (!f.second.is_target)
      PrintLongHelp(f.second.help);
  }

  if (is_markdown) {
    OutputString(
        "## <a name=\"predefined_variables\"></a>"
        "Built-in predefined variables\n\n");
  }
  for (const auto& v : variables::GetBuiltinVariables())
    PrintLongHelp(v.second.help);

  if (is_markdown) {
    OutputString(
        "## <a name=\"target_variables\"></a>"
        "Variables you set in targets\n\n");
  }
  for (const auto& v : variables::GetTargetVariables())
    PrintLongHelp(v.second.help);

  if (is_markdown)
    OutputString("## <a name=\"other\"></a>Other help topics\n\n");
  PrintLongHelp(kBuildArgs_Help, "buildargs");
  PrintLongHelp(kDotfile_Help, "dotfile");
  PrintLongHelp(kExecution_Help, "execution");
  PrintLongHelp(kGrammar_Help, "grammar");
  PrintLongHelp(kInputOutputConversion_Help, "input_conversion");
  PrintLongHelp(kLabelPattern_Help, "label_pattern");
  PrintLongHelp(kLabels_Help, "labels");
  PrintLongHelp(kNinjaRules_Help, "ninja_rules");
  PrintLongHelp(kNoGnCheck_Help, "nogncheck");
  PrintLongHelp(kInputOutputConversion_Help, "output_conversion");
  PrintLongHelp(kRuntimeDeps_Help, "runtime_deps");
  PrintLongHelp(kSourceExpansion_Help, "source_expansion");

  if (is_markdown)
    OutputString("## <a name=\"switches\"></a>Command Line Switches\n\n");
  PrintSwitchHelp();
}

// Prints help on the given switch. There should be no leading hyphens. Returns
// true if the switch was found and help was printed. False means the switch is
// unknown.
bool PrintHelpOnSwitch(const std::string& what) {
  const switches::SwitchInfoMap& all = switches::GetSwitches();
  switches::SwitchInfoMap::const_iterator found =
      all.find(base::StringPiece(what));
  if (found == all.end())
    return false;
  PrintLongHelp(found->second.long_help);
  return true;
}

}  // namespace

const char kHelp[] = "help";
const char kHelp_HelpShort[] = "help: Does what you think.";
const char kHelp_Help[] =
    R"(gn help <anything>

  Yo dawg, I heard you like help on your help so I put help on the help in the
  help.

  You can also use "all" as the parameter to get all help at once.

Switches

  --markdown
      Format output in markdown syntax.

Example

  gn help --markdown all
      Dump all help to stdout in markdown format.
)";

// Given a help text, tries to extract topics covered in it.
std::vector<base::StringPiece> ExtractTopics(base::StringPiece help) {
  // FIXME: `help tool` has 4-indent, `help dotfile`, `help toolchain` has 2-indent
  bool last_line_was_empty = true;
  std::vector<base::StringPiece> topics;

  for (const auto& line : base::SplitStringPiece(help, "\r\n",
                                                 base::KEEP_WHITESPACE,
                                                 base::SPLIT_WANT_ALL)) {

    // Look for several patterns.
    // 1. Line starting with two spaces followed by a single word, optionally
    //    followed by [text in brackets]. Detects things like
    //    |  secondary_source [optional]|
    //    from e.g. `gn help dotfile`
    if (last_line_was_empty && line.size() > 2 && line[0] == ' ' && line[1]) {
      size_t word_end = 2;
      while (word_end < line.size() && line[word_end] != ' ')
        ++word_end;

      bool found_topic = word_end == line.size();
      if (!found_topic && line[word_end + 1] == '[') {
        size_t brackets = word_end + 1;
        while (brackets < line.size() && line[brackets] != ']')
          ++brackets;
        if (brackets < line.size() && line[brackets] == ']')
          found_topic = true;
      }

      if (found_topic)
        topics.push_back(base::StringPiece(line.data() + 2, word_end - 2));
    }

    // 2. Line starting with four spaces containing a single {{word}}.
    //    Detects things like
    //    |    {{asmflags}}|
    //    from e.g. `gn help tool`
    if (last_line_was_empty &&
        base::StartsWith(line, "    {{",
                         base::CompareCase::INSENSITIVE_ASCII) &&
        base::EndsWith(line, "}}", base::CompareCase::INSENSITIVE_ASCII)) {
      bool is_single_word = true;
      for (size_t i = strlen("    {{"); i < line.size() - strlen("}}"); ++i) {
        if (base::IsAsciiWhitespace(line[i]))
          is_single_word = false;
      }
      if (!is_single_word)
        topics.push_back(base::StringPiece(line.data() + 6, line.size() - 8));
    }

    last_line_was_empty = line.empty();
  }

  return topics;
}

void SeeAlso(base::StringPiece key,
             base::StringPiece help,
             std::map<base::StringPiece, base::StringPiece>* see_also) {
  std::vector<base::StringPiece> topics = ExtractTopics(help);
  if (topics.empty())
    return;
  printf("for %s\n", key.as_string().c_str());
  for (const auto& topic : topics) {
    printf("  got %s\n", topic.as_string().c_str());
  }

  // XXX fill see_also
}

int RunHelp(const std::vector<std::string>& args) {
  std::string what;
  if (args.size() == 0) {
    // If no argument is specified, check for switches to allow things like
    // "gn help --args" for help on the args switch.
    const base::CommandLine::SwitchMap& switches =
        base::CommandLine::ForCurrentProcess()->GetSwitches();
    if (switches.empty()) {
      // Still nothing, show help overview.
      PrintToplevelHelp();
      return 0;
    }

    // Switch help needs to be done separately. The CommandLine will strip the
    // switch separators so --args will come out as "args" which is then
    // ambiguous with the variable named "args".
    if (!PrintHelpOnSwitch(switches.begin()->first))
      PrintToplevelHelp();
    return 0;
  } else {
    what = args[0];
  }

  std::vector<base::StringPiece> all_help_topics;

  std::map<base::StringPiece, base::StringPiece> see_also;

  // Check commands.
  const commands::CommandInfoMap& command_map = commands::GetCommands();
  auto found_command = command_map.find(what);
  if (found_command != command_map.end()) {
    PrintLongHelp(found_command->second.help);
    return 0;
  }
  for (const auto& entry : command_map) {
    all_help_topics.push_back(entry.first);
    SeeAlso(entry.first, entry.second.help, &see_also);
  }

  // Check functions.
  const functions::FunctionInfoMap& function_map = functions::GetFunctions();
  auto found_function = function_map.find(what);
  if (found_function != function_map.end())
    PrintLongHelp(found_function->second.help);
  for (const auto& entry : function_map) {
    all_help_topics.push_back(entry.first);
    SeeAlso(entry.first, entry.second.help, &see_also);
  }

  // Builtin variables.
  const variables::VariableInfoMap& builtin_vars =
      variables::GetBuiltinVariables();
  auto found_builtin_var = builtin_vars.find(what);
  if (found_builtin_var != builtin_vars.end())
    PrintLongHelp(found_builtin_var->second.help);
  for (const auto& entry : builtin_vars) {
    all_help_topics.push_back(entry.first);
    SeeAlso(entry.first, entry.second.help, &see_also);
  }

  // Target variables.
  const variables::VariableInfoMap& target_vars =
      variables::GetTargetVariables();
  auto found_target_var = target_vars.find(what);
  if (found_target_var != target_vars.end())
    PrintLongHelp(found_target_var->second.help);
  for (const auto& entry : target_vars) {
    all_help_topics.push_back(entry.first);
    SeeAlso(entry.first, entry.second.help, &see_also);
  }

// XXX
  ExtractTopics(kDotfile_Help);

  // XXX maybe remove this
  if (found_function != function_map.end() ||
      found_builtin_var != builtin_vars.end() ||
      found_target_var != target_vars.end())
    return 0;

  // Random other topics.
  std::map<std::string, void (*)()> random_topics;
  random_topics["all"] = PrintAllHelp;
#define AddLongHelp(key, help) \
  SeeAlso(key, help, &see_also); \
  random_topics[key] = []() { PrintLongHelp(help); }
  AddLongHelp("execution", kExecution_Help);
  AddLongHelp("buildargs", kBuildArgs_Help);
  AddLongHelp("dotfile", kDotfile_Help);
  AddLongHelp("grammar", kGrammar_Help);
  AddLongHelp("input_conversion", kInputOutputConversion_Help);
  AddLongHelp("label_pattern", kLabelPattern_Help);
  AddLongHelp("labels", kLabels_Help);
  AddLongHelp("ninja_rules", kNinjaRules_Help);
  AddLongHelp("nogncheck", kNoGnCheck_Help);
  AddLongHelp("output_conversion", kInputOutputConversion_Help);
  AddLongHelp("runtime_deps", kRuntimeDeps_Help);
  AddLongHelp("source_expansion", kSourceExpansion_Help);
  random_topics["switches"] = PrintSwitchHelp;
  auto found_random_topic = random_topics.find(what);
  if (found_random_topic != random_topics.end()) {
    found_random_topic->second();
    return 0;
  }
  for (const auto& entry : random_topics)
    all_help_topics.push_back(entry.first);

  // No help on this.
  Err(Location(), "No help on \"" + what + "\".").PrintToStdout();
  base::StringPiece suggestion = SpellcheckString(what, all_help_topics);
  if (suggestion.empty()) {
    OutputString("Run `gn help` for a list of available topics.\n",
                 DECORATION_NONE);
  } else {
    OutputString("Did you mean `gn help " + suggestion.as_string() + "`?\n",
                 DECORATION_NONE);
  }
  return 1;
}

}  // namespace commands
