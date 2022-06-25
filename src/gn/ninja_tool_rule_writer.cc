// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_tool_rule_writer.h"

#include <fstream>

#include "gn/c_tool.h"
#include "gn/escape.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_utils.h"
#include "gn/pool.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/trace.h"

namespace {

const char kIndent[] = "  ";

}  // namespace

NinjaToolRuleWriter::NinjaToolRuleWriter(const Settings* settings,
                                         const Tool* tool,
                                         std::ostream& out)
    : settings_(settings), tool_(tool), out_(out) {}

NinjaToolRuleWriter::~NinjaToolRuleWriter() = default;

// static
void NinjaToolRuleWriter::WriteToolRule(const Settings* settings,
                                        const Tool* tool,
                                        std::ostream& out) {
  std::string rule_prefix = GetNinjaRulePrefixForToolchain(settings);
  WriteToolRuleWithName(rule_prefix + tool->name(), settings, tool, out);
}

// static
void NinjaToolRuleWriter::WriteToolRuleWithName(const std::string& rule_name,
                                                const Settings* settings,
                                                const Tool* tool,
                                                std::ostream& out) {
  NinjaToolRuleWriter gen(settings, tool, out);
  gen.WriteRule(rule_name);
}

void NinjaToolRuleWriter::WriteRule(const std::string& rule_name) {
  out_ << "rule " << rule_name << std::endl;

  // Rules explicitly include shell commands, so don't try to escape.
  EscapeOptions options;
  options.mode = ESCAPE_NINJA_PREFORMATTED_COMMAND;

  WriteCommandRulePattern("command", tool_->command_launcher(),
                          tool_->command(), options);

  WriteRulePattern("description", tool_->description(), options);
  WriteRulePattern("rspfile", tool_->rspfile(), options);
  WriteRulePattern("rspfile_content", tool_->rspfile_content(), options);

  if (const CTool* c_tool = tool_->AsC()) {
    if (c_tool->depsformat() == CTool::DEPS_GCC) {
      // GCC-style deps require a depfile.
      if (!c_tool->depfile().empty()) {
        WriteRulePattern("depfile", tool_->depfile(), options);
        out_ << kIndent << "deps = gcc" << std::endl;
      }
    } else if (c_tool->depsformat() == CTool::DEPS_MSVC) {
      // MSVC deps don't have a depfile.
      out_ << kIndent << "deps = msvc" << std::endl;
    }
  } else if (!tool_->depfile().empty()) {
    WriteRulePattern("depfile", tool_->depfile(), options);
    out_ << kIndent << "deps = gcc" << std::endl;
  }

  // Use pool is specified.
  if (tool_->pool().ptr) {
    std::string pool_name =
        tool_->pool().ptr->GetNinjaName(settings_->default_toolchain_label());
    out_ << kIndent << "pool = " << pool_name << std::endl;
  }

  if (tool_->restat())
    out_ << kIndent << "restat = 1" << std::endl;
}

void NinjaToolRuleWriter::WriteRulePattern(const char* name,
                                           const SubstitutionPattern& pattern,
                                           const EscapeOptions& options) {
  if (pattern.empty())
    return;
  out_ << kIndent << name << " = ";
  SubstitutionWriter::WriteWithNinjaVariables(pattern, options, out_);
  out_ << std::endl;
}

void NinjaToolRuleWriter::WriteCommandRulePattern(
    const char* name,
    const std::string& launcher,
    const SubstitutionPattern& command,
    const EscapeOptions& options) {
  CHECK(!command.empty()) << "Command should not be empty";
  out_ << kIndent << name << " = ";
  if (!launcher.empty())
    out_ << launcher << " ";
  SubstitutionWriter::WriteWithNinjaVariables(command, options, out_);
  out_ << std::endl;
}
