// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/test_with_scope.h"
#include "tools/gn/value.h"
#include "util/build_config.h"
#include "util/test/test.h"

namespace {

Value Call(Scope* scope,
           Value& targets,
           Value& data_keys,
           Value& walk_keys,
           bool rebase) {
  std::vector<Value> args;
  args.push_back(targets);
  args.push_back(data_keys);
  args.push_back(walk_keys);
  args.push_back(Value(nullptr, rebase));

  Err err;
  FunctionCallNode function;
  Value result = functions::RunGetMetadata(scope, &function, args, &err);
  EXPECT_FALSE(err.has_error()) << err.message();
  EXPECT_TRUE(result.type() == Value::OPAQUE);

  return result;
}

}  // namespace

TEST(GetMetadata, Errors) {
  TestWithScope setup;

  // No arg input should issue an error.
  Err err;
  std::vector<Value> args;
  FunctionCallNode function;
  Value ret = functions::RunGetMetadata(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(GetMetadata, ErrorNotInTarget) {
  TestWithScope setup;
  FunctionCallNode target_node;

  // Not having target_name set in scope should issue an error.
  Err err;
  Value targets(nullptr, Value::LIST);
  targets.list_value().push_back(
      Value(nullptr, "//target(//toolchain:default)"));

  Value data(nullptr, Value::LIST);
  Value walk(nullptr, Value::LIST);

  std::vector<Value> args;
  args.push_back(targets);
  args.push_back(data);
  args.push_back(walk);
  args.push_back(Value(nullptr, false));

  FunctionCallNode function;
  Value result =
      functions::RunGetMetadata(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(GetMetadata, ValidWithExplicitTarget) {
  TestWithScope setup;
  FunctionCallNode target_node;
  setup.scope()->SetValue("target_name", Value(nullptr, "target"),
                          &target_node);
  setup.scope()->set_source_dir(SourceDir("//target"));

  Err err;
  Value targets(nullptr, Value::LIST);
  targets.list_value().push_back(
      Value(nullptr, "//target(//toolchain:default)"));

  Value data(nullptr, Value::LIST);
  data.list_value().push_back(Value(nullptr, "data"));

  Value walk(nullptr, Value::LIST);
  walk.list_value().push_back(Value(nullptr, "walk"));

  Value actual = Call(setup.scope(), targets, data, walk, true);

  // TestTarget target(setup, "//target", Target::SOURCE_SET);
  // Value a_contents(nullptr, Value::LIST);
  // a_contents.list_value().push_back(Value(nullptr, true));
  // target.metadata().contents().insert(
  //     std::make_pair("data", a_contents));
  // std::set<const Target*> walked;
  // Value result = actual.opaque_value()(&target, &walked, &err);
  // EXPECT_EQ(result, a_contents) << result.ToString(false);
}

TEST(GetMetadata, ValidWithInferredTarget) {
  TestWithScope setup;
  FunctionCallNode target_node;
  setup.scope()->SetValue("target_name", Value(nullptr, "target"),
                          &target_node);
  setup.scope()->set_source_dir(SourceDir("//target"));

  Err err;
  Value targets(nullptr, Value::LIST);
  // The empty string should infer the local target.
  targets.list_value().push_back(Value(nullptr, ""));

  Value data(nullptr, Value::LIST);
  data.list_value().push_back(Value(nullptr, "data"));

  Value walk(nullptr, Value::LIST);
  walk.list_value().push_back(Value(nullptr, "walk"));

  Value value_actual = Call(setup.scope(), targets, data, walk, true);
}
