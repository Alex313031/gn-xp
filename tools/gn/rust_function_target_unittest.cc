// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/test_with_scheduler.h"
#include "tools/gn/test_with_scope.h"
#include "util/test/test.h"

using RustFunctionsTarget = TestWithScheduler;

// Checks that the appropriate crate root is found.
TEST_F(RustFunctionsTarget, CrateRootFind) {
  TestWithScope setup;

  // The target generator needs a place to put the targets or it will fail.
  Scope::ItemVector item_collector;
  setup.scope()->set_item_collector(&item_collector);
  setup.scope()->set_source_dir(SourceDir("/"));

  TestParseInput normal_input(
      "rust_executable(\"foo\") {\n"
      "  crate_root = \"foo.rs\""
      "  sources = [ \"main.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(normal_input.has_error());
  Err err;
  normal_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(
      item_collector.back()->AsTarget()->rust_values().crate_root()->value(),
      "/foo.rs");

  TestParseInput exe_input(
      "rust_executable(\"foo\") {\n"
      "  sources = [ \"foo.rs\", \"lib.rs\", \"main.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(exe_input.has_error());
  err = Err();
  exe_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(
      item_collector.back()->AsTarget()->rust_values().crate_root()->value(),
      "/main.rs");

  TestParseInput lib_input(
      "rust_library(\"libfoo\") {\n"
      "  crate_type = \"lib\"\n"
      "  sources = [ \"foo.rs\", \"lib.rs\", \"main.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(lib_input.has_error());
  err = Err();
  lib_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(
      item_collector.back()->AsTarget()->rust_values().crate_root()->value(),
      "/lib.rs");

  TestParseInput singlesource_input(
      "rust_executable(\"bar\") {\n"
      "  sources = [ \"bar.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(singlesource_input.has_error());
  err = Err();
  singlesource_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(
      item_collector.back()->AsTarget()->rust_values().crate_root()->value(),
      "/bar.rs");

  TestParseInput error_input(
      "rust_library(\"foo\") {\n"
      "  crate_type = \"lib\"\n"
      "  sources = [ \"foo.rs\", \"main.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(error_input.has_error());
  err = Err();
  error_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error());
  EXPECT_EQ("Missing \"crate_root\" and missing \"lib.rs\" in sources.",
            err.message());
}

// Checks that the appropriate crate type is used.
TEST_F(RustFunctionsTarget, CrateTypeSelection) {
  TestWithScope setup;

  // The target generator needs a place to put the targets or it will fail.
  Scope::ItemVector item_collector;
  setup.scope()->set_item_collector(&item_collector);
  setup.scope()->set_source_dir(SourceDir("/"));

  TestParseInput normal_input(
      "rust_executable(\"foo\") {\n"
      "  sources = [ \"main.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(normal_input.has_error());
  Err err;
  normal_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(item_collector.back()->AsTarget()->rust_values().crate_type(),
            "bin");

  TestParseInput exe_input(
      "rust_executable(\"foo\") {\n"
      "  crate_type = \"bin\"\n"
      "  sources = [ \"foo.rs\", \"lib.rs\", \"main.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(exe_input.has_error());
  err = Err();
  exe_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(item_collector.back()->AsTarget()->rust_values().crate_type(),
            "bin");

  TestParseInput lib_input(
      "rust_library(\"libfoo\") {\n"
      "  crate_type = \"lib\"\n"
      "  sources = [ \"lib.rs\" ]\n"
      "}\n");
  ASSERT_FALSE(lib_input.has_error());
  err = Err();
  lib_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();
  ASSERT_EQ(item_collector.back()->AsTarget()->rust_values().crate_type(),
            "lib");

  TestParseInput exe_error_input(
      "rust_executable(\"foo\") {\n"
      "  crate_type = \"lib\"\n"
      "}\n");
  ASSERT_FALSE(exe_error_input.has_error());
  err = Err();
  exe_error_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error());
  EXPECT_EQ("Inadmissible crate type lib.", err.message());

  TestParseInput lib_error_input(
      "rust_library(\"foo\") {\n"
      "  crate_type = \"bin\"\n"
      "}\n");
  ASSERT_FALSE(lib_error_input.has_error());
  err = Err();
  lib_error_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error());
  EXPECT_EQ("Inadmissible crate type bin.", err.message());

  TestParseInput lib_missing_error_input(
      "rust_library(\"foo\") {\n"
      "}\n");
  ASSERT_FALSE(lib_missing_error_input.has_error());
  err = Err();
  lib_missing_error_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error());
  EXPECT_EQ("Must set \"crate_type\" on a \"rust_library\".", err.message());

  TestParseInput error_input(
      "rust_library(\"foo\") {\n"
      "  crate_type = \"evil\"\n"
      "}\n");
  ASSERT_FALSE(error_input.has_error());
  err = Err();
  error_input.parsed()->Execute(setup.scope(), &err);
  ASSERT_TRUE(err.has_error());
  EXPECT_EQ("Inadmissible crate type evil.", err.message());
}
