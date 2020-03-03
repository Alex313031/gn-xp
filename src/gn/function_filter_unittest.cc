// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/functions.h"
#include "gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(FilterTest, FilterExclude) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value exclude_patterns(nullptr, Value::LIST);
  exclude_patterns.list_value().push_back(Value(nullptr, "*.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values, exclude_patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 2);
  EXPECT_EQ(result.list_value()[0].type(), Value::STRING);
  EXPECT_EQ(result.list_value()[0].string_value(), "foo.cc");
  EXPECT_EQ(result.list_value()[1].type(), Value::STRING);
  EXPECT_EQ(result.list_value()[1].string_value(), "foo.h");
}

TEST(FilterTest, FilterExcludeAndInclude) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value exclude_patterns(nullptr, Value::LIST);
  exclude_patterns.list_value().push_back(Value(nullptr, "*"));

  Value include_patterns(nullptr, Value::LIST);
  include_patterns.list_value().push_back(Value(nullptr, "*.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values, exclude_patterns, include_patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  ASSERT_FALSE(err.has_error());
  ASSERT_EQ(result.type(), Value::LIST);
  ASSERT_EQ(result.list_value().size(), 1);
  EXPECT_EQ(result.list_value()[0].type(), Value::STRING);
  EXPECT_EQ(result.list_value()[0].string_value(), "foo.proto");
}

TEST(FilterTest, IncorrectNotEnoughArguments) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(FilterTest, IncorrectTooManyArguments) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value exclude_patterns(nullptr, Value::LIST);
  exclude_patterns.list_value().push_back(Value(nullptr, "*"));

  Value include_patterns(nullptr, Value::LIST);
  include_patterns.list_value().push_back(Value(nullptr, "*.proto"));

  Value extra_argument(nullptr, Value::LIST);

  FunctionCallNode function;
  std::vector<Value> args = {values, exclude_patterns, include_patterns,
                             extra_argument};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(FilterTest, IncorrectValuesType) {
  TestWithScope setup;

  Value values(nullptr, "foo.cc");

  Value exclude_patterns(nullptr, Value::LIST);
  exclude_patterns.list_value().push_back(Value(nullptr, "*"));

  Value include_patterns(nullptr, Value::LIST);
  include_patterns.list_value().push_back(Value(nullptr, "*.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values, exclude_patterns, include_patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(FilterTest, IncorrectExcludePatternsType) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value exclude_patterns(nullptr, "foo.cc");

  Value include_patterns(nullptr, Value::LIST);
  include_patterns.list_value().push_back(Value(nullptr, "*.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values, exclude_patterns, include_patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(FilterTest, IncorrectIncludePatternsType) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value exclude_patterns(nullptr, Value::LIST);
  exclude_patterns.list_value().push_back(Value(nullptr, "*"));

  Value include_patterns(nullptr, "*.proto");

  FunctionCallNode function;
  std::vector<Value> args = {values, exclude_patterns, include_patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}
