// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/unique_key.h"

#include "util/test/test.h"

#include <set>

TEST(UniqueKeyTest, EmptyString) {
  UniqueKey key1;
  UniqueKey key2("");

  ASSERT_STREQ(key1.str().c_str(), "");
  ASSERT_STREQ(key2.str().c_str(), "");
  ASSERT_EQ(&key1.str(), &key2.str());
}

TEST(UniqueKeyTest, Find) {
  UniqueKey empty;
  EXPECT_EQ(empty.str(), std::string());

  UniqueKey foo("foo");
  EXPECT_EQ(foo.str(), std::string("foo"));

  UniqueKey foo2("foo");
  EXPECT_EQ(&foo.str(), &foo2.str());
}

// Default compare should always be ordered.
TEST(UniqueKeyTest, DefaultCompare) {
  auto foo = UniqueKey("foo");
  auto bar = UniqueKey("bar");
  auto zoo = UniqueKey("zoo");

  EXPECT_TRUE(bar < foo);
  EXPECT_TRUE(foo < zoo);
  EXPECT_TRUE(bar < zoo);
}

TEST(UniqueKeyTest, NormalSet) {
  std::set<UniqueKey> set;
  auto foo_ret = set.insert(std::string_view("foo"));
  auto bar_ret = set.insert(std::string_view("bar"));
  auto zoo_ret = set.insert(std::string_view("zoo"));

  UniqueKey foo_key("foo");
  EXPECT_EQ(*foo_ret.first, foo_key);

  auto foo_it = set.find(foo_key);
  EXPECT_NE(foo_it, set.end());
  EXPECT_EQ(*foo_it, foo_key);

  EXPECT_EQ(set.find(std::string_view("bar")), bar_ret.first);
  EXPECT_EQ(set.find(std::string_view("zoo")), zoo_ret.first);

  // Normal sets are always ordered according to the key value.
  auto it = set.begin();
  EXPECT_EQ(it, bar_ret.first);
  ++it;

  EXPECT_EQ(it, foo_ret.first);
  ++it;

  EXPECT_EQ(it, zoo_ret.first);
  ++it;

  EXPECT_EQ(it, set.end());
}

TEST(UniqueKeyTest, FastSet) {
  std::set<UniqueKey, UniqueKey::FastCompare> set;

  auto foo_ret = set.insert(std::string_view("foo"));
  auto bar_ret = set.insert(std::string_view("bar"));
  auto zoo_ret = set.insert(std::string_view("zoo"));

  UniqueKey foo_key("foo");
  EXPECT_EQ(*foo_ret.first, foo_key);

  auto foo_it = set.find(foo_key);
  EXPECT_NE(foo_it, set.end());
  EXPECT_EQ(*foo_it, foo_key);

  EXPECT_EQ(set.find(std::string_view("bar")), bar_ret.first);
  EXPECT_EQ(set.find(std::string_view("zoo")), zoo_ret.first);

  // Fast sets are ordered according to the key pointer.
  // Because of the underlying bump allocator, addresses
  // for the first three inserts are in increasing order.
  auto it = set.begin();
  EXPECT_EQ(it, foo_ret.first);
  ++it;

  EXPECT_EQ(it, bar_ret.first);
  ++it;

  EXPECT_EQ(it, zoo_ret.first);
  ++it;

  EXPECT_EQ(it, set.end());
}
