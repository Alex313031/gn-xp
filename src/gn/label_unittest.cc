// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <iterator>

#include "base/macros.h"
#include "gn/err.h"
#include "gn/label.h"
#include "gn/value.h"
#include "util/build_config.h"
#include "util/test/test.h"

namespace {

struct ParseDepStringCase {
  const char* cur_dir;
  const char* str;
  bool success;
  const char* expected_dir;
  const char* expected_name;
  const char* expected_toolchain_dir;
  const char* expected_toolchain_name;
};

bool CompareParseResult(Label::ParseResult& a, const Label::ParseResult& b) {
  bool success = true;

  // Compare two field values and print a useful error message to stderr
  // when they differ.
  auto compare_field = [&](const char* name, std::string_view a_val,
                           std::string_view b_val) {
    if (a_val != b_val) {
      fprintf(stderr, "ParseResult::%s [%s] != [%s]\n", name,
              std::string(a_val).c_str(), std::string(b_val).c_str());
      success = false;
    }
  };

  compare_field("location", a.location, b.location);
  compare_field("name", a.name, b.name);
  compare_field("toolchain", a.toolchain, b.toolchain);

  // The error field is a const char*, not an std::string_view value.
  if (a.error != b.error) {
    fprintf(stderr, "ParseResult::error [%s] != [%s]",
            (a.error ? std::string(a.error).c_str() : "NULL"),
            (b.error ? std::string(b.error).c_str() : "NULL"));
    success = false;
  }

  // NOTE: Field error_text intentionally ignored here.

  return success;
}

}  // namespace

TEST(Label, Resolve) {
  ParseDepStringCase cases[] = {
    {"//chrome/", "", false, "", "", "", ""},
    {"//chrome/", "/", false, "", "", "", ""},
    {"//chrome/", ":", false, "", "", "", ""},
    {"//chrome/", "/:", false, "", "", "", ""},
    {"//chrome/", "blah", true, "//chrome/blah/", "blah", "//t/", "d"},
    {"//chrome/", "blah:bar", true, "//chrome/blah/", "bar", "//t/", "d"},
    // Absolute paths.
    {"//chrome/", "/chrome:bar", true, "/chrome/", "bar", "//t/", "d"},
    {"//chrome/", "/chrome/:bar", true, "/chrome/", "bar", "//t/", "d"},
#if defined(OS_WIN)
    {"//chrome/", "/C:/chrome:bar", true, "/C:/chrome/", "bar", "//t/", "d"},
    {"//chrome/", "/C:/chrome/:bar", true, "/C:/chrome/", "bar", "//t/", "d"},
    {"//chrome/", "C:/chrome:bar", true, "/C:/chrome/", "bar", "//t/", "d"},
#endif
    // Refers to root dir.
    {"//chrome/", "//:bar", true, "//", "bar", "//t/", "d"},
    // Implicit directory
    {"//chrome/", ":bar", true, "//chrome/", "bar", "//t/", "d"},
    {"//chrome/renderer/", ":bar", true, "//chrome/renderer/", "bar", "//t/",
     "d"},
    // Implicit names.
    {"//chrome/", "//base", true, "//base/", "base", "//t/", "d"},
    {"//chrome/", "//base/i18n", true, "//base/i18n/", "i18n", "//t/", "d"},
    {"//chrome/", "//base/i18n:foo", true, "//base/i18n/", "foo", "//t/", "d"},
    {"//chrome/", "//", false, "", "", "", ""},
    // Toolchain parsing.
    {"//chrome/", "//chrome:bar(//t:n)", true, "//chrome/", "bar", "//t/", "n"},
    {"//chrome/", "//chrome:bar(//t)", true, "//chrome/", "bar", "//t/", "t"},
    {"//chrome/", "//chrome:bar(//t:)", true, "//chrome/", "bar", "//t/", "t"},
    {"//chrome/", "//chrome:bar()", true, "//chrome/", "bar", "//t/", "d"},
    {"//chrome/", "//chrome:bar(foo)", true, "//chrome/", "bar",
     "//chrome/foo/", "foo"},
    {"//chrome/", "//chrome:bar(:foo)", true, "//chrome/", "bar", "//chrome/",
     "foo"},
    // TODO(brettw) it might be nice to make this an error:
    //{"//chrome/", "//chrome:bar())", false, "", "", "", "" },
    {"//chrome/", "//chrome:bar(//t:bar(tc))", false, "", "", "", ""},
    {"//chrome/", "//chrome:bar(()", false, "", "", "", ""},
    {"//chrome/", "(t:b)", false, "", "", "", ""},
    {"//chrome/", ":bar(//t/b)", true, "//chrome/", "bar", "//t/b/", "b"},
    {"//chrome/", ":bar(/t/b)", true, "//chrome/", "bar", "/t/b/", "b"},
    {"//chrome/", ":bar(t/b)", true, "//chrome/", "bar", "//chrome/t/b/", "b"},
  };

  ToolchainLabel default_toolchain(SourceDir("//t/"), "d");

  for (size_t i = 0; i < std::size(cases); i++) {
    const ParseDepStringCase& cur = cases[i];

    std::string location, name;
    Err err;
    Value v(nullptr, Value::STRING);
    v.string_value() = cur.str;
    Label result = Label::Resolve(SourceDir(cur.cur_dir), std::string_view(),
                                  default_toolchain, v, &err);
    EXPECT_EQ(cur.success, !err.has_error()) << i << " " << cur.str;
    if (!err.has_error() && cur.success) {
      EXPECT_EQ(cur.expected_dir, result.dir().value()) << i << " " << cur.str;
      EXPECT_EQ(cur.expected_name, result.name()) << i << " " << cur.str;
      EXPECT_EQ(cur.expected_toolchain_dir, result.toolchain().dir().value())
          << i << " " << cur.str;
      EXPECT_EQ(cur.expected_toolchain_name, result.toolchain().name())
          << i << " " << cur.str << " expected " << cur.expected_toolchain_name
          << " got " << result.toolchain().name() << " toolchain ["
          << result.toolchain().str() << "]";
    }
  }
}

TEST(Label, ParseLabelString) {
  const struct {
    const char* input;
    Label::ParseResult expected;
    bool allow_toolchain = false;
  } kData[] = {
    // clang-format off
    // Empty string
    { "", {} },
    // Directory only
    { "//foo/bar", { .location = "//foo/bar" } },
    { "foo", { .location = "foo" } },
    { "/foo", { .location = "/foo" } },
#if defined(OS_WIN)
    { "C:/foo", { .location = "C:/foo" } },
    { "/C:foo/bar", { .location = "/C:foo/bar" } },
#endif
    // Name only
    { ":foo", { .name = "foo", } },
    // Directory and name.
    { "//foo:bar", { .location = "//foo", .name = "bar" } },
    { "//foo/bar/zoo:tool", { .location = "//foo/bar/zoo", .name = "tool" } },
    // Directory + name + toolchain.
    {
      "foo/bar:zoo(//build/toolchain)",
      { .location = "foo/bar", .name = "zoo", .toolchain = "//build/toolchain" },
      true,
    },
    // Name with toolchain
    { .input = ":foo(toolchain)",
      .expected = {
        // NOTE: The error message comes from the fact that this error only
        // happens in practice when using a toolchain name inside a label
        // toolchain (because the toolchain named extracted from a first
        // call to Label::ParseLabelString() is then parsed by a second
        // call to Label::ParseLabelString().
        .error = "Toolchain has a toolchain.",
      },
      .allow_toolchain = false,
    },
    {
      .input = ":foo(toolchain)",
      .expected = { .name = "foo", .toolchain = "toolchain" },
      .allow_toolchain = true,
    },
    // Bad toolchain end
    {
      "//foo:bar(toolchain",
      { .error = "Bad toolchain name" },
      true,
    }
    // clang-format on
  };
  for (const auto& data : kData) {
    Label::ParseResult result =
        Label::ParseLabelString(data.input, data.allow_toolchain);
    EXPECT_TRUE(CompareParseResult(result, data.expected))
        << "input [" << data.input
        << "] allow_toolchain=" << (data.allow_toolchain ? "true" : "false");
  }
}

// Tests the case where the path resolves to something above "//". It should get
// converted to an absolute path "/foo/bar".
TEST(Label, ResolveAboveRootBuildDir) {
  ToolchainLabel default_toolchain(SourceDir("//t/"), "d");

  std::string location, name;
  Err err;

  SourceDir cur_dir("//cur/");
  std::string source_root("/foo/bar/baz");

  // No source root given, should not go above the root build dir.
  Label result = Label::Resolve(cur_dir, std::string_view(), default_toolchain,
                                Value(nullptr, "../../..:target"), &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ("//", result.dir().value()) << result.dir().value();
  EXPECT_EQ("target", result.name());

  // Source root provided, it should go into that.
  result = Label::Resolve(cur_dir, source_root, default_toolchain,
                          Value(nullptr, "../../..:target"), &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ("/foo/", result.dir().value()) << result.dir().value();
  EXPECT_EQ("target", result.name());

  // It should't go up higher than the system root.
  result = Label::Resolve(cur_dir, source_root, default_toolchain,
                          Value(nullptr, "../../../../..:target"), &err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ("/", result.dir().value()) << result.dir().value();
  EXPECT_EQ("target", result.name());

  // Test an absolute label that goes above the source root. This currently
  // stops at the source root. It should arguably keep going and produce "/foo/"
  // but this test just makes sure the current behavior isn't regressed by
  // accident.
  result = Label::Resolve(cur_dir, source_root, default_toolchain,
                          Value(nullptr, "//../.."), &err);
  EXPECT_FALSE(err.has_error()) << err.message();
  EXPECT_EQ("/foo/", result.dir().value()) << result.dir().value();
  EXPECT_EQ("foo", result.name());
}
