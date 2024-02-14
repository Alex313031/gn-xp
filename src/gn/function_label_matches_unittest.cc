// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(LabelMatchesTest, MatchesSubTarget) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//foo:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::BOOLEAN);
  ASSERT_EQ(result.boolean_value(), true);
}

TEST(LabelMatchesTest, MatchesTargetInFile) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//bar:foo"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::BOOLEAN);
  ASSERT_EQ(result.boolean_value(), true);
}

TEST(LabelMatchesTest, NoMatch) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  args.push_back(Value(nullptr, "//baz/foo:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunLabelMatches(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::BOOLEAN);
  ASSERT_EQ(result.boolean_value(), false);
}

TEST(FilterLabelsTest, OneIncluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabels(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(1u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//foo:bar", result.list_value()[0].string_value());
}

TEST(FilterLabelsTest, TwoIncluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//foo/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabels(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(2u, result.list_value().size());
  ASSERT_TRUE(result.list_value()[0].type() == Value::STRING);
  ASSERT_EQ("//foo:bar", result.list_value()[0].string_value());
  ASSERT_TRUE(result.list_value()[1].type() == Value::STRING);
  ASSERT_EQ("//bar", result.list_value()[1].string_value());
}

TEST(FilterLabelsTest, NoneIncluded) {
  TestWithScope setup;
  FunctionCallNode function;

  std::vector<Value> args;
  Value labels(nullptr, Value::LIST);
  labels.list_value().push_back(Value(nullptr, "//foo:bar"));
  labels.list_value().push_back(Value(nullptr, "//baz:bar"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "//fooz/*"));
  patterns.list_value().push_back(Value(nullptr, "//bar:*"));

  args.push_back(labels);
  args.push_back(patterns);

  Err err;
  Value result =
      functions::RunFilterLabels(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(0u, result.list_value().size());
}