// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "tools/gn/test_with_scope.h"
#include "tools/gn/value.h"
#include "util/test/test.h"

TEST(Value, ToString) {
  Value strval(nullptr, "hi\" $me\\you\\$\\\"");
  EXPECT_EQ("hi\" $me\\you\\$\\\"", strval.ToString(false));
  EXPECT_EQ("\"hi\\\" \\$me\\you\\\\\\$\\\\\\\"\"", strval.ToString(true));

  // crbug.com/470217
  Value strval2(nullptr, "\\foo\\\\bar\\");
  EXPECT_EQ("\"\\foo\\\\\\bar\\\\\"", strval2.ToString(true));

  // Void type.
  EXPECT_EQ("<void>", Value().ToString(false));

  // Test lists, bools, and ints.
  Value listval(nullptr, Value::LIST);
  listval.list_value().push_back(Value(nullptr, "hi\"me"));
  listval.list_value().push_back(Value(nullptr, true));
  listval.list_value().push_back(Value(nullptr, false));
  listval.list_value().push_back(Value(nullptr, static_cast<int64_t>(42)));
  // Printing lists always causes embedded strings to be quoted (ignoring the
  // quote flag), or else they wouldn't make much sense.
  EXPECT_EQ("[\"hi\\\"me\", true, false, 42]", listval.ToString(false));
  EXPECT_EQ("[\"hi\\\"me\", true, false, 42]", listval.ToString(true));

  // Scopes.
  TestWithScope setup;
  Scope* scope = new Scope(setup.scope());
  Value scopeval(nullptr, std::unique_ptr<Scope>(scope));
  EXPECT_EQ("{ }", scopeval.ToString(false));

  // Test that an empty scope equals an empty scope.
  EXPECT_TRUE(scopeval == scopeval);

  scope->SetValue("a", Value(nullptr, static_cast<int64_t>(42)), nullptr);
  scope->SetValue("b", Value(nullptr, "hello, world"), nullptr);
  EXPECT_EQ("{\n  a = 42\n  b = \"hello, world\"\n}", scopeval.ToString(false));
  EXPECT_TRUE(scopeval == scopeval);

  Scope* inner_scope = new Scope(setup.scope());
  Value inner_scopeval(nullptr, std::unique_ptr<Scope>(inner_scope));
  inner_scope->SetValue("d", Value(nullptr, static_cast<int64_t>(42)),
                         nullptr);
  scope->SetValue("c", inner_scopeval, nullptr);

  // Test inner scope equality.
  EXPECT_TRUE(scopeval == scopeval);

  // Test parent/child scope equality.
  Scope* parent_scope = new Scope(setup.scope());
  Value parent_scopeval(nullptr, std::unique_ptr<Scope>(parent_scope));
  parent_scope->SetValue("a", Value(nullptr, static_cast<int64_t>(42)), nullptr);

  Scope* child_scope = new Scope(parent_scope);
  Value child_scopeval(nullptr, std::unique_ptr<Scope>(child_scope));
  child_scope->SetValue("b", Value(nullptr, "hello, world"), nullptr);

  Scope* grandchild_scope = new Scope(child_scope);
  Value grandchild_scopeval(nullptr, std::unique_ptr<Scope>(grandchild_scope));
  grandchild_scope->SetValue("c", Value(nullptr, true), nullptr);

  Scope* other_parent_scope = new Scope(setup.scope());
  Value otherparent_scopeval(nullptr, std::unique_ptr<Scope>(other_parent_scope));
  other_parent_scope->SetValue("a", Value(nullptr, static_cast<int64_t>(42)), nullptr);
  other_parent_scope->SetValue("b", Value(nullptr, "hello, world"), nullptr);


  Scope* other_child_scope = new Scope(other_parent_scope);
  Value other_child_scopeval(nullptr, std::unique_ptr<Scope>(other_child_scope));
  other_child_scope->SetValue("c", Value(nullptr, true), nullptr);

  EXPECT_TRUE(grandchild_scopeval == other_child_scopeval);
}
