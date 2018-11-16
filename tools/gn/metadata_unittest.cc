// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/metadata.h"
#include "tools/gn/test_with_scope.h"
#include "util/test/test.h"

TEST(MetadataTest, SetContents) {
  Metadata metadata;

  ASSERT_TRUE(metadata.contents().empty());

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "foo"));
  Value b_expected(nullptr, Value::LIST);
  b_expected.list_value().push_back(Value(nullptr, true));

  Metadata::Contents contents;
  contents.insert(std::pair<base::StringPiece, Value>("a", a_expected));
  contents.insert(std::pair<base::StringPiece, Value>("b", b_expected));

  metadata.set_contents(std::move(contents));

  ASSERT_EQ(metadata.contents().size(), 2);
  auto a_actual = metadata.contents().find("a");
  auto b_actual = metadata.contents().find("b");
  ASSERT_FALSE(a_actual == metadata.contents().end());
  ASSERT_FALSE(b_actual == metadata.contents().end());
  ASSERT_EQ(a_actual->second, a_expected);
  ASSERT_EQ(b_actual->second, b_expected);
}

TEST(MetadataTest, Walk) {
  Metadata metadata;
  metadata.set_source_dir(SourceDir("/usr/home/files/"));

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "foo.cpp"));
  a_expected.list_value().push_back(Value(nullptr, "bar.h"));

  metadata.contents().insert(
      std::pair<base::StringPiece, Value>("a", a_expected));

  std::vector<base::StringPiece> data_keys;
  data_keys.emplace_back("a");
  std::vector<base::StringPiece> walk_keys;
  std::vector<base::StringPiece> next_walk_keys;
  std::vector<Value> results;

  std::vector<Value> expected;
  expected.emplace_back(Value(nullptr, "foo.cpp"));
  expected.emplace_back(Value(nullptr, "bar.h"));

  std::vector<base::StringPiece> expected_walk_keys;
  expected_walk_keys.emplace_back("");

  Err err;
  metadata.walk(data_keys, walk_keys, next_walk_keys, results, err);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(next_walk_keys, expected_walk_keys);
  EXPECT_EQ(results, expected);
}

TEST(MetadataTest, WalkWithRebase) {
  Metadata metadata;
  metadata.set_source_dir(SourceDir("/usr/home/files/"));

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "foo.cpp"));
  a_expected.list_value().push_back(Value(nullptr, "foo/bar.h"));

  metadata.contents().insert(
      std::pair<base::StringPiece, Value>("a", a_expected));

  std::vector<base::StringPiece> data_keys;
  data_keys.emplace_back("a");
  std::vector<base::StringPiece> walk_keys;
  std::vector<base::StringPiece> next_walk_keys;
  std::vector<Value> results;

  std::vector<Value> expected;
  expected.emplace_back(Value(nullptr, "/usr/home/files/foo.cpp"));
  expected.emplace_back(Value(nullptr, "/usr/home/files/foo/bar.h"));

  std::vector<base::StringPiece> expected_walk_keys;
  expected_walk_keys.emplace_back("");

  Err err;
  metadata.walk(data_keys, walk_keys, next_walk_keys, results, err, true);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(next_walk_keys, expected_walk_keys);
  EXPECT_EQ(results, expected);
}

TEST(MetadataTest, WalkWithRebaseError) {
  Metadata metadata;
  metadata.set_source_dir(SourceDir("/usr/home/files/"));

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "foo.cpp"));
  a_expected.list_value().push_back(Value(nullptr, true));

  metadata.contents().insert(
      std::pair<base::StringPiece, Value>("a", a_expected));

  std::vector<base::StringPiece> data_keys;
  data_keys.emplace_back("a");
  std::vector<base::StringPiece> walk_keys;
  std::vector<base::StringPiece> next_walk_keys;
  std::vector<Value> results;

  Err err;
  metadata.walk(data_keys, walk_keys, next_walk_keys, results, err, true);
  EXPECT_TRUE(err.has_error());
}

TEST(MetadataTest, WalkKeysToWalk) {
  Metadata metadata;
  metadata.set_source_dir(SourceDir("/usr/home/files/"));

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "//target"));

  metadata.contents().insert(
      std::pair<base::StringPiece, Value>("a", a_expected));

  std::vector<base::StringPiece> data_keys;
  std::vector<base::StringPiece> walk_keys;
  walk_keys.emplace_back("a");
  std::vector<base::StringPiece> next_walk_keys;
  std::vector<Value> results;

  std::vector<base::StringPiece> expected_walk_keys;
  expected_walk_keys.emplace_back("//target");

  Err err;
  metadata.walk(data_keys, walk_keys, next_walk_keys, results, err, true);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(next_walk_keys, expected_walk_keys);
  EXPECT_TRUE(results.empty());
}

TEST(MetadataTest, WalkNoContents) {
  Metadata metadata;
  metadata.set_source_dir(SourceDir("/usr/home/files/"));

  std::vector<base::StringPiece> data_keys;
  std::vector<base::StringPiece> walk_keys;
  std::vector<base::StringPiece> next_walk_keys;
  std::vector<Value> results;

  std::vector<base::StringPiece> expected_walk_keys;
  expected_walk_keys.emplace_back("");

  Err err;
  metadata.walk(data_keys, walk_keys, next_walk_keys, results, err, true);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(next_walk_keys, expected_walk_keys);
  EXPECT_TRUE(results.empty());
}

TEST(MetadataTest, WalkNoKeysWithContents) {
  Metadata metadata;
  metadata.set_source_dir(SourceDir("/usr/home/files/"));

  Value a_expected(nullptr, Value::LIST);
  a_expected.list_value().push_back(Value(nullptr, "//target"));

  metadata.contents().insert(
      std::pair<base::StringPiece, Value>("a", a_expected));

  std::vector<base::StringPiece> data_keys;
  std::vector<base::StringPiece> walk_keys;
  std::vector<base::StringPiece> next_walk_keys;
  std::vector<Value> results;

  std::vector<base::StringPiece> expected_walk_keys;
  expected_walk_keys.emplace_back("");

  Err err;
  metadata.walk(data_keys, walk_keys, next_walk_keys, results, err, true);
  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(next_walk_keys, expected_walk_keys);
  EXPECT_TRUE(results.empty());
}
