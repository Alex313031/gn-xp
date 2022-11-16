// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/resolved_target_data.h"

#include "gn/test_with_scope.h"
#include "util/test/test.h"

// Tests that lib[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST(ResolvedTargetDataTest, LibInheritance) {
  TestWithScope setup;
  Err err;

  ResolvedTargetData resolved;

  const LibFile lib("foo");
  const SourceDir libdir("/foo_dir/");

  // Leaf target with ldflags set.
  TestTarget z(setup, "//foo:z", Target::STATIC_LIBRARY);
  z.config_values().libs().push_back(lib);
  z.config_values().lib_dirs().push_back(libdir);
  ASSERT_TRUE(z.OnResolved(&err));

  // All lib[_dir]s should be set when target is resolved.
  const auto& all_libs = resolved.GetLinkedLibraries(&z);
  ASSERT_EQ(1u, all_libs.size());
  EXPECT_EQ(lib, all_libs[0]);

  const auto& all_lib_dirs = resolved.GetLinkedLibraryDirs(&z);
  ASSERT_EQ(1u, all_lib_dirs.size());
  EXPECT_EQ(libdir, all_lib_dirs[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const LibFile second_lib("bar");
  const SourceDir second_libdir("/bar_dir/");
  TestTarget shared(setup, "//foo:shared", Target::SHARED_LIBRARY);
  shared.config_values().libs().push_back(second_lib);
  shared.config_values().lib_dirs().push_back(second_libdir);
  shared.private_deps().push_back(LabelTargetPair(&z));
  ASSERT_TRUE(shared.OnResolved(&err));

  const auto& all_libs2 = resolved.GetLinkedLibraries(&shared);
  ASSERT_EQ(2u, all_libs2.size());
  EXPECT_EQ(second_lib, all_libs2[0]);
  EXPECT_EQ(lib, all_libs2[1]);

  const auto& all_lib_dirs2 = resolved.GetLinkedLibraryDirs(&shared);
  ASSERT_EQ(2u, all_lib_dirs2.size());
  EXPECT_EQ(second_libdir, all_lib_dirs2[0]);
  EXPECT_EQ(libdir, all_lib_dirs2[1]);

  // Executable target shouldn't get either by depending on shared.
  TestTarget exec(setup, "//foo:exec", Target::EXECUTABLE);
  exec.private_deps().push_back(LabelTargetPair(&shared));
  ASSERT_TRUE(exec.OnResolved(&err));

  const auto& all_libs3 = resolved.GetLinkedLibraries(&exec);
  EXPECT_EQ(0u, all_libs3.size());

  const auto& all_lib_dirs3 = resolved.GetLinkedLibraryDirs(&exec);
  EXPECT_EQ(0u, all_lib_dirs3.size());
}

// Tests that framework[_dir]s are inherited across deps boundaries for static
// libraries but not executables.
TEST(ResolvedTargetDataTest, FrameworkInheritance) {
  TestWithScope setup;
  Err err;

  const std::string framework("Foo.framework");
  const SourceDir frameworkdir("//out/foo/");

  // Leaf target with ldflags set.
  TestTarget z(setup, "//foo:z", Target::STATIC_LIBRARY);
  z.config_values().frameworks().push_back(framework);
  z.config_values().framework_dirs().push_back(frameworkdir);
  ASSERT_TRUE(z.OnResolved(&err));

  ResolvedTargetData resolved;

  // All framework[_dir]s should be set when target is resolved.
  const auto& frameworks = resolved.GetLinkedFrameworks(&z);
  ASSERT_EQ(1u, frameworks.size());
  EXPECT_EQ(framework, frameworks[0]);

  const auto& framework_dirs = resolved.GetLinkedFrameworkDirs(&z);
  ASSERT_EQ(1u, framework_dirs.size());
  EXPECT_EQ(frameworkdir, framework_dirs[0]);

  // Shared library target should inherit the libs from the static library
  // and its own. Its own flag should be before the inherited one.
  const std::string second_framework("Bar.framework");
  const SourceDir second_frameworkdir("//out/bar/");
  TestTarget shared(setup, "//foo:shared", Target::SHARED_LIBRARY);
  shared.config_values().frameworks().push_back(second_framework);
  shared.config_values().framework_dirs().push_back(second_frameworkdir);
  shared.private_deps().push_back(LabelTargetPair(&z));
  ASSERT_TRUE(shared.OnResolved(&err));

  const auto& frameworks2 = resolved.GetLinkedFrameworks(&shared);
  ASSERT_EQ(2u, frameworks2.size());
  EXPECT_EQ(second_framework, frameworks2[0]);
  EXPECT_EQ(framework, frameworks2[1]);

  const auto& framework_dirs2 = resolved.GetLinkedFrameworkDirs(&shared);
  ASSERT_EQ(2u, framework_dirs2.size());
  EXPECT_EQ(second_frameworkdir, framework_dirs2[0]);
  EXPECT_EQ(frameworkdir, framework_dirs2[1]);

  // Executable target shouldn't get either by depending on shared.
  TestTarget exec(setup, "//foo:exec", Target::EXECUTABLE);
  exec.private_deps().push_back(LabelTargetPair(&shared));
  ASSERT_TRUE(exec.OnResolved(&err));

  const auto& frameworks3 = resolved.GetLinkedFrameworks(&exec);
  EXPECT_EQ(0u, frameworks3.size());

  const auto& framework_dirs3 = resolved.GetLinkedFrameworkDirs(&exec);
  EXPECT_EQ(0u, framework_dirs3.size());
}

TEST(ResolvedTargetDataTest, InheritLibs) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (shared lib) -> C (static lib) -> D (source set)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::SHARED_LIBRARY);
  TestTarget c(setup, "//foo:c", Target::STATIC_LIBRARY);
  TestTarget d(setup, "//foo:d", Target::SOURCE_SET);
  a.private_deps().push_back(LabelTargetPair(&b));
  b.private_deps().push_back(LabelTargetPair(&c));
  c.private_deps().push_back(LabelTargetPair(&d));

  ASSERT_TRUE(d.OnResolved(&err));
  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  // C should have D in its inherited libs.
  const auto& c_inherited_libs = resolved.GetInheritedLibraries(&c);
  ASSERT_EQ(1u, c_inherited_libs.size());
  EXPECT_EQ(&d, c_inherited_libs[0].target());

  // B should have C and D in its inherited libs.
  const auto& b_inherited = resolved.GetInheritedLibraries(&b);
  ASSERT_EQ(2u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());
  EXPECT_EQ(&d, b_inherited[1].target());

  // A should have B in its inherited libs, but not any others (the shared
  // library will include the static library and source set).
  const auto& a_inherited = resolved.GetInheritedLibraries(&a);
  ASSERT_EQ(1u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());
}

TEST(ResolvedTargetData, NoActionDepPropgation) {
  TestWithScope setup;
  Err err;
  ResolvedTargetData resolved;
  // Create a dependency chain:
  //   A (exe) -> B (action) -> C (source_set)
  {
    TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
    TestTarget b(setup, "//foo:b", Target::ACTION);
    TestTarget c(setup, "//foo:c", Target::SOURCE_SET);

    a.private_deps().push_back(LabelTargetPair(&b));
    b.private_deps().push_back(LabelTargetPair(&c));

    ASSERT_TRUE(c.OnResolved(&err));
    ASSERT_TRUE(b.OnResolved(&err));
    ASSERT_TRUE(a.OnResolved(&err));

    // The executable should not have inherited the source set across the
    // action.
    ASSERT_TRUE(resolved.GetInheritedLibraries(&a).empty());
  }
}

TEST(ResolvedTargetDataTest, InheritCompleteStaticLib) {
  TestWithScope setup;
  Err err;

  ResolvedTargetData resolved;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (source set)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::STATIC_LIBRARY);
  b.set_complete_static_lib(true);

  const LibFile lib("foo");
  const SourceDir lib_dir("/foo_dir/");
  TestTarget c(setup, "//foo:c", Target::SOURCE_SET);
  c.config_values().libs().push_back(lib);
  c.config_values().lib_dirs().push_back(lib_dir);

  a.public_deps().push_back(LabelTargetPair(&b));
  b.public_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  // B should have C in its inherited libs.
  const auto& b_inherited = resolved.GetInheritedLibraries(&b);
  ASSERT_EQ(1u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());

  // A should have B in its inherited libs, but not any others (the complete
  // static library will include the source set).
  const auto& a_inherited = resolved.GetInheritedLibraries(&a);
  ASSERT_EQ(1u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());

  // A should inherit the libs and lib_dirs from the C.
  const auto& a_libs = resolved.GetLinkedLibraries(&a);
  ASSERT_EQ(1u, a_libs.size());
  EXPECT_EQ(lib, a_libs[0]);

  const auto& a_lib_dirs = resolved.GetLinkedLibraryDirs(&a);
  ASSERT_EQ(1u, a_lib_dirs.size());
  EXPECT_EQ(lib_dir, a_lib_dirs[0]);
}

TEST(ResolvedTargetDataTest, InheritCompleteStaticLibStaticLibDeps) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (static lib)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::STATIC_LIBRARY);
  b.set_complete_static_lib(true);
  TestTarget c(setup, "//foo:c", Target::STATIC_LIBRARY);
  a.public_deps().push_back(LabelTargetPair(&b));
  b.public_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  // B should have C in its inherited libs.
  const auto& b_inherited = resolved.GetInheritedLibraries(&b);
  ASSERT_EQ(1u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());

  // A should have B in its inherited libs, but not any others (the complete
  // static library will include the static library).
  const auto& a_inherited = resolved.GetInheritedLibraries(&a);
  ASSERT_EQ(1u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());
}

TEST(ResolvedTargetDataTest,
     InheritCompleteStaticLibInheritedCompleteStaticLibDeps) {
  TestWithScope setup;
  Err err;

  // Create a dependency chain:
  //   A (executable) -> B (complete static lib) -> C (complete static lib)
  TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
  TestTarget b(setup, "//foo:b", Target::STATIC_LIBRARY);
  b.set_complete_static_lib(true);
  TestTarget c(setup, "//foo:c", Target::STATIC_LIBRARY);
  c.set_complete_static_lib(true);

  a.private_deps().push_back(LabelTargetPair(&b));
  b.private_deps().push_back(LabelTargetPair(&c));

  ASSERT_TRUE(c.OnResolved(&err));
  ASSERT_TRUE(b.OnResolved(&err));
  ASSERT_TRUE(a.OnResolved(&err));

  ResolvedTargetData resolved;

  // B should have C in its inherited libs.
  const auto& b_inherited = resolved.GetInheritedLibraries(&b);
  ASSERT_EQ(1u, b_inherited.size());
  EXPECT_EQ(&c, b_inherited[0].target());

  // A should have B and C in its inherited libs.
  const auto& a_inherited = resolved.GetInheritedLibraries(&a);
  ASSERT_EQ(2u, a_inherited.size());
  EXPECT_EQ(&b, a_inherited[0].target());
  EXPECT_EQ(&c, a_inherited[1].target());
}

TEST(ResolvedTargetDataTest, NoActionDepPropgation) {
  TestWithScope setup;
  Err err;
  ResolvedTargetData resolved;

  // Create a dependency chain:
  //   A (exe) -> B (action) -> C (source_set)
  {
    TestTarget a(setup, "//foo:a", Target::EXECUTABLE);
    TestTarget b(setup, "//foo:b", Target::ACTION);
    TestTarget c(setup, "//foo:c", Target::SOURCE_SET);

    a.private_deps().push_back(LabelTargetPair(&b));
    b.private_deps().push_back(LabelTargetPair(&c));

    ASSERT_TRUE(c.OnResolved(&err));
    ASSERT_TRUE(b.OnResolved(&err));
    ASSERT_TRUE(a.OnResolved(&err));

    // The executable should not have inherited the source set across the
    // action.
    const auto& libs = resolved.GetInheritedLibraries(&a);
    ASSERT_TRUE(libs.empty());
  }
}
