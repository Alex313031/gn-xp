// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_toolchain_writer.h"

#include <fstream>

#include "base/files/file_util.h"
#include "gn/builtin_tool.h"
#include "gn/c_tool.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_tool_rule_writer.h"
#include "gn/ninja_utils.h"
#include "gn/pool.h"
#include "gn/settings.h"
#include "gn/toolchain.h"
#include "gn/trace.h"

NinjaToolchainWriter::NinjaToolchainWriter(const Settings* settings,
                                           const Toolchain* toolchain,
                                           std::ostream& out)
    : settings_(settings),
      toolchain_(toolchain),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   settings_->build_settings()->root_path_utf8(),
                   ESCAPE_NINJA) {}

NinjaToolchainWriter::~NinjaToolchainWriter() = default;

void NinjaToolchainWriter::Run(
    const std::vector<NinjaWriter::TargetRulePair>& rules) {
  for (const auto& tool : toolchain_->tools()) {
    if (tool.second->name() == GeneralTool::kGeneralToolAction ||
        tool.second->AsBuiltin()) {
      continue;
    }
    NinjaToolRuleWriter::WriteToolRule(settings_, tool.second.get(), out_);
  }
  out_ << std::endl;

  for (const auto& pair : rules)
    out_ << pair.second;
}

// static
bool NinjaToolchainWriter::RunAndWriteFile(
    const Settings* settings,
    const Toolchain* toolchain,
    const std::vector<NinjaWriter::TargetRulePair>& rules) {
  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      GetNinjaFileForToolchain(settings)));
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, FilePathToUTF8(ninja_file));

  base::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  NinjaToolchainWriter gen(settings, toolchain, file);
  gen.Run(rules);
  return true;
}
