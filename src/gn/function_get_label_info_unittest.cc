// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/test/test.h"

namespace {

class GetLabelInfoTest : public testing::Test {
 public:
  GetLabelInfoTest() : testing::Test() {
    setup_.scope()->set_source_dir(SourceDir("//src/foo/"));
  }

  // Convenience wrapper to call GetLabelInfo.
  std::string Call(const std::string& label, const std::string& what) {
    std::vector<Value> args;
    args.push_back(Value(nullptr, label));
    args.push_back(Value(nullptr, what));

    Err err;
    Value result =
        functions::RunGetLabelInfo(setup_.scope(), &function_, args, &err);
    if (err.has_error()) {
      EXPECT_TRUE(result.type() == Value::NONE);
      return std::string();
    }
    return result.string_value();
  }

 protected:
  // Note: TestWithScope's default toolchain is "//toolchain:default" and
  // output dir is "//out/Debug".
  TestWithScope setup_;
  FunctionCallNode function_;
};

}  // namespace

TEST_F(GetLabelInfoTest, BadInput) {
  EXPECT_EQ("", Call(":name", "incorrect_value"));
  EXPECT_EQ("", Call("", "name"));
}

TEST_F(GetLabelInfoTest, Name) {
  EXPECT_EQ("name", Call(":name", "name"));
  EXPECT_EQ("name", Call("//foo/bar:name", "name"));
  EXPECT_EQ("name", Call("//foo/bar:name(//other:tc)", "name"));
}

TEST_F(GetLabelInfoTest, Dir) {
  EXPECT_EQ("//src/foo", Call(":name", "dir"));
  EXPECT_EQ("//foo/bar", Call("//foo/bar:baz", "dir"));
  EXPECT_EQ("//foo/bar", Call("//foo/bar:baz(//other:tc)", "dir"));
}

TEST_F(GetLabelInfoTest, RootOutDir) {
  EXPECT_EQ("//out/Debug", Call(":name", "root_out_dir"));
  EXPECT_EQ("//out/Debug/random",
            Call(":name(//toolchain:random)", "root_out_dir"));
}

TEST_F(GetLabelInfoTest, RootGenDir) {
  EXPECT_EQ("//out/Debug/gen", Call(":name", "root_gen_dir"));
  EXPECT_EQ("//out/Debug/gen",
            Call(":name(//toolchain:default)", "root_gen_dir"));
  EXPECT_EQ("//out/Debug/random/gen",
            Call(":name(//toolchain:random)", "root_gen_dir"));
}

TEST_F(GetLabelInfoTest, TargetOutDir) {
  EXPECT_EQ("//out/Debug/obj/src/foo", Call(":name", "target_out_dir"));
  EXPECT_EQ("//out/Debug/obj/foo",
            Call("//foo:name(//toolchain:default)", "target_out_dir"));
  EXPECT_EQ("//out/Debug/random/obj/foo",
            Call("//foo:name(//toolchain:random)", "target_out_dir"));
}

TEST_F(GetLabelInfoTest, TargetGenDir) {
  EXPECT_EQ("//out/Debug/gen/src/foo", Call(":name", "target_gen_dir"));
  EXPECT_EQ("//out/Debug/gen/foo",
            Call("//foo:name(//toolchain:default)", "target_gen_dir"));
  EXPECT_EQ("//out/Debug/random/gen/foo",
            Call("//foo:name(//toolchain:random)", "target_gen_dir"));
}

TEST_F(GetLabelInfoTest, LabelNoToolchain) {
  EXPECT_EQ("//src/foo:name", Call(":name", "label_no_toolchain"));
  EXPECT_EQ("//src/foo:name",
            Call("//src/foo:name(//toolchain:random)", "label_no_toolchain"));
}

TEST_F(GetLabelInfoTest, LabelWithToolchain) {
  EXPECT_EQ("//src/foo:name(//toolchain:default)",
            Call(":name", "label_with_toolchain"));
  EXPECT_EQ("//src/foo:name(//toolchain:random)",
            Call(":name(//toolchain:random)", "label_with_toolchain"));
}

TEST_F(GetLabelInfoTest, Toolchain) {
  EXPECT_EQ("//toolchain:default", Call(":name", "toolchain"));
  EXPECT_EQ("//toolchain:random",
            Call(":name(//toolchain:random)", "toolchain"));
}

TEST_F(GetLabelInfoTest, All) {
  const std::string label = "//src/foo:baz(//toolchain:random)";
  std::vector<Value> args = { Value(nullptr, label), Value(nullptr, "all") };

  Err err;
  const Value result =
      functions::RunGetLabelInfo(setup_.scope(), &function_, args, &err);

  EXPECT_EQ(Value::SCOPE, result.type());
  const Scope* scope = result.scope_value();
  EXPECT_TRUE(scope != nullptr);

  EXPECT_EQ(Call(label, "name"), scope->GetValue("name")->string_value());
  EXPECT_EQ(Call(label, "dir"), scope->GetValue("dir")->string_value());
  EXPECT_EQ(Call(label, "root_out_dir"),
            scope->GetValue("root_out_dir")->string_value());
  EXPECT_EQ(Call(label, "root_gen_dir"),
            scope->GetValue("root_gen_dir")->string_value());
  EXPECT_EQ(Call(label, "target_out_dir"),
            scope->GetValue("target_out_dir")->string_value());
  EXPECT_EQ(Call(label, "target_gen_dir"),
            scope->GetValue("target_gen_dir")->string_value());
  EXPECT_EQ(Call(label, "toolchain"),
            scope->GetValue("toolchain")->string_value());
  EXPECT_EQ(Call(label, "label_with_toolchain"),
            scope->GetValue("label_with_toolchain")->string_value());
  EXPECT_EQ(Call(label, "label_no_toolchain"),
            scope->GetValue("label_no_toolchain")->string_value());
}
