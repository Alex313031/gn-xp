// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/nested_set.h"
#include "util/test/test.h"

#include <cstdio>

using NestedStringSet = NestedSet<std::string>;

TEST(NestedSet, DefaultConstructor) {
  NestedStringSet set1;
  EXPECT_TRUE(set1.empty());
  EXPECT_EQ(0u, set1.deps_count());
  EXPECT_EQ(0u, set1.items_count());
  EXPECT_EQ(0u, set1.ref_count());
}

TEST(NestedSet, BuilderConstruction) {
  NestedStringSet::Builder builder;
  builder.AddItem(std::string("A"));
  builder.AddItem(std::string("B"));
  NestedStringSet set_ab = builder.Build();

  EXPECT_EQ(2u, set_ab.items_count());
  EXPECT_EQ(std::string("A"), set_ab.items()[0]);
  EXPECT_EQ(std::string("B"), set_ab.items()[1]);
  EXPECT_EQ(0u, set_ab.deps_count());
  EXPECT_EQ(1u, set_ab.ref_count());

  // Builder does not reference set_ab.
  builder.Reset();
  EXPECT_EQ(1u, set_ab.ref_count());
}

TEST(NestedSet, CopyOperations) {
  NestedStringSet::Builder builder;
  builder.AddItem(std::string("A"));
  builder.AddItem(std::string("B"));
  NestedStringSet set_ab = builder.Build();

  EXPECT_EQ(1u, set_ab.ref_count());

  // Copy construction.
  NestedStringSet set_copy(set_ab);
  EXPECT_EQ(2u, set_ab.ref_count());
  EXPECT_EQ(2u, set_copy.ref_count());
  EXPECT_EQ(set_ab, set_copy);

  // Copy assignment.
  NestedStringSet set_assign;
  set_assign = set_ab;
  EXPECT_EQ(3u, set_ab.ref_count());
  EXPECT_EQ(3u, set_copy.ref_count());
  EXPECT_EQ(3u, set_assign.ref_count());
  EXPECT_EQ(set_ab, set_assign);
}

TEST(NestedSet, MoveOperations) {
  NestedStringSet::Builder builder;
  builder.AddItem(std::string("A"));
  builder.AddItem(std::string("B"));
  NestedStringSet set_ab = builder.Build();

  // Move construction.
  NestedStringSet set_move(std::move(set_ab));
  EXPECT_EQ(0u, set_ab.ref_count());
  EXPECT_EQ(1u, set_move.ref_count());
  EXPECT_NE(set_ab, set_move);
  EXPECT_TRUE(set_ab.empty());

  // Move assignment.
  NestedStringSet set_assign;
  set_assign = std::move(set_move);
  EXPECT_EQ(0u, set_ab.ref_count());
  EXPECT_EQ(0u, set_move.ref_count());
  EXPECT_EQ(1u, set_assign.ref_count());
  EXPECT_NE(set_ab, set_assign);
  EXPECT_NE(set_move, set_assign);
  EXPECT_EQ(set_ab, set_move);
  EXPECT_TRUE(set_move.empty());
}

TEST(NestedSet, Flatten) {
  NestedStringSet::Builder builder;

  // Helper lambda to conver a vector of strings to a string.
  auto to_string = [](const std::vector<std::string>& items) {
    std::string result;
    for (const auto& item : items) {
      if (!result.empty())
        result += ' ';
      result += item;
    }
    // printf("[%s]\n", result.c_str());  // DEBUG
    return result;
  };

  // Helper macro to compare the result of a Flatten() call to
  // an actual string literal (after passing it to to_string())
  // and printing the differences in case of failure.
#define EXPECT_FLATTENED(expected_str, value)                               \
  do {                                                                      \
    auto actual = to_string(value);                                         \
    EXPECT_EQ(std::string(expected_str), actual);                           \
    if (actual != expected_str)                                             \
      fprintf(stderr, "  ACTUAL   [%s]\n  EXPECTED [%s]\n", actual.c_str(), \
              expected_str);                                                \
  } while (0)

  //    A    A -> B C  Default: ABCD
  //   / \   B -> D    Include: DBCA
  //  B   C  C -> D    Link:    ABCD
  //   \ /             Legacy:  ABDC
  //    D
  {
    NestedStringSet d = builder.Reset().AddItem("d1").AddItem("d2").Build();

    NestedStringSet b =
        builder.Reset().AddItem("b1").AddItem("b2").AddDep(d).Build();

    NestedStringSet c =
        builder.Reset().AddItem("c1").AddItem("c2").AddDep(d).Build();

    NestedStringSet a =
        builder.Reset().AddItem("a1").AddItem("a2").AddDep(b).AddDep(c).Build();

    EXPECT_FLATTENED("a1 a2 b1 b2 c1 c2 d1 d2",
                     a.Flatten(NestedSetOrder::Default));

    EXPECT_FLATTENED("d1 d2 b1 b2 c1 c2 a1 a2",
                     a.Flatten(NestedSetOrder::Include));

    EXPECT_FLATTENED("a1 a2 b1 b2 c1 c2 d1 d2",
                     a.Flatten(NestedSetOrder::Link));

    EXPECT_FLATTENED("a1 a2 b1 b2 d1 d2 c1 c2",
                     a.Flatten(NestedSetOrder::Legacy));
  }
  //    A    A -> B C  Default: ABCEDF
  //   / \   B -> E D  Include: FEDBCA
  //  B   C  C -> D F  Link:    ABECDF
  //  |\ /|  E -> F    Legacy:  ABEFDC
  //  E D |
  //  \   /
  //   \ /
  //    F
  {
    NestedStringSet f = builder.Reset().AddItem("f").Build();

    NestedStringSet e = builder.Reset().AddItem("e").AddDep(f).Build();

    NestedStringSet d = builder.Reset().AddItem("d").Build();

    NestedStringSet b =
        builder.Reset().AddItem("b").AddDep(e).AddDep(d).Build();

    NestedStringSet c =
        builder.Reset().AddItem("c").AddDep(d).AddDep(f).Build();

    NestedStringSet a =
        builder.Reset().AddItem("a").AddDep(b).AddDep(c).Build();

    EXPECT_FLATTENED("a b c e d f", a.Flatten(NestedSetOrder::Default));

    EXPECT_FLATTENED("f e d b c a", a.Flatten(NestedSetOrder::Include));

    EXPECT_FLATTENED("a b e c d f", a.Flatten(NestedSetOrder::Link));

    EXPECT_FLATTENED("a b e f d c", a.Flatten(NestedSetOrder::Legacy));
  }
  //    A    A -> B C  Default:  ABCFDE
  //   / \   B -> F D  Include:  FDBECA
  //  B   C  C -> D E  Link:     ABCDEF
  //  |\ /|  E -> F    Legacy:   ABFDCE
  //  | D E
  //  \   /
  //   \ /
  //    F
  {
    NestedStringSet f = builder.Reset().AddItem("f").Build();

    NestedStringSet e = builder.Reset().AddItem("e").AddDep(f).Build();

    NestedStringSet d = builder.Reset().AddItem("d").Build();

    NestedStringSet b =
        builder.Reset().AddItem("b").AddDep(f).AddDep(d).Build();

    NestedStringSet c =
        builder.Reset().AddItem("c").AddDep(d).AddDep(e).Build();

    NestedStringSet a =
        builder.Reset().AddItem("a").AddDep(b).AddDep(c).Build();

    EXPECT_FLATTENED("a b c f d e", a.Flatten(NestedSetOrder::Default));

    EXPECT_FLATTENED("f d b e c a", a.Flatten(NestedSetOrder::Include));

    EXPECT_FLATTENED("a b c d e f", a.Flatten(NestedSetOrder::Link));

    EXPECT_FLATTENED("a b f d c e", a.Flatten(NestedSetOrder::Legacy));
  }
  //  A     A -> B C
  //  |\    B -> D C
  //  B \   C -> D
  //  |\ |
  //  | \|    Default:  ABCD
  //   \ C    Include:  DCBA
  //    \|    Link:     ABCD
  //     D    Legacy:   ABDC
  //
  {
    NestedStringSet d = builder.Reset().AddItem("d").Build();

    NestedStringSet c = builder.Reset().AddItem("c").AddDep(d).Build();

    NestedStringSet b =
        builder.Reset().AddItem("b").AddDep(d).AddDep(c).Build();

    NestedStringSet a =
        builder.Reset().AddItem("a").AddDep(b).AddDep(c).Build();

    EXPECT_FLATTENED("a b c d", a.Flatten(NestedSetOrder::Default));

    EXPECT_FLATTENED("d c b a", a.Flatten(NestedSetOrder::Include));

    EXPECT_FLATTENED("a b c d", a.Flatten(NestedSetOrder::Link));

    EXPECT_FLATTENED("a b d c", a.Flatten(NestedSetOrder::Legacy));
  }
  //  A     A -> B C
  //  |\    B -> D
  //  | \   C -> B D
  //  |  C
  //  | /|
  //  |/ |
  //  B  |    Default:  ABCD
  //   \ |    Include:  DBCA
  //    \|    Link:     ACBD
  //     D    Legacy:   ABDC
  {
    NestedStringSet d = builder.Reset().AddItem("d").Build();

    NestedStringSet b = builder.Reset().AddItem("b").AddDep(d).Build();

    NestedStringSet c =
        builder.Reset().AddItem("c").AddDep(b).AddDep(d).Build();

    NestedStringSet a =
        builder.Reset().AddItem("a").AddDep(b).AddDep(c).Build();

    EXPECT_FLATTENED("a b c d", a.Flatten(NestedSetOrder::Default));

    EXPECT_FLATTENED("d b c a", a.Flatten(NestedSetOrder::Include));

    EXPECT_FLATTENED("a c b d", a.Flatten(NestedSetOrder::Link));

    EXPECT_FLATTENED("a b d c", a.Flatten(NestedSetOrder::Legacy));
  }
}
