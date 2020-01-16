// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"
#include "tools/gn/test_with_scope.h"
#include "util/build_config.h"
#include "util/test/test.h"

TEST(FilterTest, Filter) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "*.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values, patterns};

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

TEST(FilterTest, IncorrectNumberOfArguments) {
  TestWithScope setup;

  FunctionCallNode function;
  std::vector<Value> args = {};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(FilterTest, IncorrectValuesType) {
  TestWithScope setup;

  Value values(nullptr, "foo.cc");

  Value patterns(nullptr, Value::LIST);
  patterns.list_value().push_back(Value(nullptr, "*.proto"));

  FunctionCallNode function;
  std::vector<Value> args = {values, patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}

TEST(FilterTest, IncorrectPatternsType) {
  TestWithScope setup;

  Value values(nullptr, Value::LIST);
  values.list_value().push_back(Value(nullptr, "foo.cc"));
  values.list_value().push_back(Value(nullptr, "foo.h"));
  values.list_value().push_back(Value(nullptr, "foo.proto"));

  Value patterns(nullptr, "*.proto");

  FunctionCallNode function;
  std::vector<Value> args = {values, patterns};

  Err err;
  Value result = functions::RunFilter(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}
