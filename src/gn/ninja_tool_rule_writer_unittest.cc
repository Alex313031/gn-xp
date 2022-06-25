// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "gn/ninja_tool_rule_writer.h"
#include "gn/settings.h"
#include "gn/test_with_scope.h"
#include "gn/toolchain.h"
#include "util/test/test.h"

TEST(NinjaToolRuleWriter, WriteToolRule) {
  TestWithScope setup;

  std::ostringstream stream;
  NinjaToolRuleWriter::WriteToolRule(
      setup.settings(), setup.toolchain()->GetTool(CTool::kCToolCc), stream);

  EXPECT_EQ(
      "rule cc\n"
      "  command = cc ${in} ${cflags} ${cflags_c} ${defines} ${include_dirs} "
      "-o ${out}\n",
      stream.str());
}

TEST(NinjaToolRuleWriter, WriteToolRuleWithLauncher) {
  TestWithScope setup;

  std::ostringstream stream;
  NinjaToolRuleWriter::WriteToolRule(
      setup.settings(), setup.toolchain()->GetTool(CTool::kCToolCxx), stream);

  EXPECT_EQ(
      "rule cxx\n"
      "  command = launcher c++ ${in} ${cflags} ${cflags_cc} ${defines} "
      "${include_dirs} "
      "-o ${out}\n",
      stream.str());
}

TEST(NinjaToolRuleWriter, WriteToolRuleWithNonDefaultToolchain) {
  TestWithScope setup;
  Settings non_default_settings(setup.build_settings(), "alt_toolchain/");
  Toolchain non_default_toolchain(
      &non_default_settings,
      Label(SourceDir("//alt_toolchain/"), "alt_toolchain"));
  TestWithScope::SetupToolchain(&non_default_toolchain);
  non_default_settings.set_toolchain_label(non_default_toolchain.label());
  non_default_settings.set_default_toolchain_label(setup.toolchain()->label());

  std::ostringstream stream;
  NinjaToolRuleWriter::WriteToolRule(
      &non_default_settings, non_default_toolchain.GetTool(CTool::kCToolCc),
      stream);

  EXPECT_EQ(
      "rule alt_toolchain_cc\n"
      "  command = cc ${in} ${cflags} ${cflags_c} ${defines} ${include_dirs} "
      "-o ${out}\n",
      stream.str());
}

TEST(NinjaToolRuleWriter, WriteToolRuleWithCustomName) {
  TestWithScope setup;

  std::ostringstream stream;
  NinjaToolRuleWriter::WriteToolRuleWithName(
      "custom_name", setup.settings(),
      setup.toolchain()->GetTool(CTool::kCToolCc), stream);

  EXPECT_EQ(
      "rule custom_name\n"
      "  command = cc ${in} ${cflags} ${cflags_c} ${defines} ${include_dirs} "
      "-o ${out}\n",
      stream.str());
}
