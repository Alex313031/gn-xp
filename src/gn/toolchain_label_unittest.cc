// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/toolchain_label.h"

#include "gn/source_dir.h"

#include "util/build_config.h"
#include "util/test/test.h"

TEST(ToolchainLabel, DefaultConstructor) {
  ToolchainLabel label;

  EXPECT_TRUE(label.empty());
  EXPECT_STREQ(label.str().c_str(), "");
  EXPECT_TRUE(label.dir().is_null());
  EXPECT_TRUE(label.name().empty());
  EXPECT_STREQ(label.GetOutputDir().c_str(), "");
}

TEST(ToolchainLabel, Constructor) {
  SourceDir dir("//foo/bar/");
  std::string_view name = "target";

  ToolchainLabel label(dir, name);
  EXPECT_FALSE(label.empty());
  EXPECT_STREQ(label.str().c_str(), "//foo/bar:target");
  EXPECT_EQ(label.dir(), dir);
  EXPECT_EQ(label.name(), name);
  EXPECT_STREQ(label.GetOutputDir().c_str(), (std::string(name) + "/").c_str())
      << "output dir [" << label.GetOutputDir() << "]";
}