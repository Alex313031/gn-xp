// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_write_data_target_writer.h"

#include "tools/gn/source_file.h"
#include "tools/gn/target.h"
#include "tools/gn/test_with_scheduler.h"
#include "tools/gn/test_with_scope.h"
#include "util/test/test.h"

using NinjaWriteDataTargetWriterTest = TestWithScheduler;

TEST_F(NinjaWriteDataTargetWriterTest, Run) {
  Err err;
  TestWithScope setup;

  Target target(setup.settings(), Label(SourceDir("//foo/"), "bar"));
  OutputFile output(setup.build_settings(),
                    target.output_dir().ResolveRelativeFile(
                        Value(nullptr, "foo.json"), &err,
                        setup.build_settings()->root_path_utf8()));
  target.set_output_type(Target::WRITE_DATA);
  target.visibility().SetPublic();
  target.set_write_data_output(output);
  target.set_write_data(Value(nullptr, true));
  target.set_should_write(false);
  target.set_write_output_conversion(Value(nullptr, "json"));

  Target dep(setup.settings(), Label(SourceDir("//foo/"), "dep"));
  dep.set_output_type(Target::ACTION);
  dep.visibility().SetPublic();
  dep.SetToolchain(setup.toolchain());
  ASSERT_TRUE(dep.OnResolved(&err));

  Target dep2(setup.settings(), Label(SourceDir("//foo/"), "dep2"));
  dep2.set_output_type(Target::ACTION);
  dep2.visibility().SetPublic();
  dep2.SetToolchain(setup.toolchain());
  ASSERT_TRUE(dep2.OnResolved(&err));

  Target datadep(setup.settings(), Label(SourceDir("//foo/"), "datadep"));
  datadep.set_output_type(Target::ACTION);
  datadep.visibility().SetPublic();
  datadep.SetToolchain(setup.toolchain());
  ASSERT_TRUE(datadep.OnResolved(&err));

  target.public_deps().push_back(LabelTargetPair(&dep));
  target.public_deps().push_back(LabelTargetPair(&dep2));
  target.data_deps().push_back(LabelTargetPair(&datadep));

  target.SetToolchain(setup.toolchain());
  ASSERT_TRUE(target.OnResolved(&err)) << err.message();

  std::ostringstream out;
  NinjaWriteDataTargetWriter writer(&target, out);
  writer.Run();

  const char expected[] =
      "build obj/foo/bar.stamp: stamp obj/foo/dep.stamp obj/foo/dep2.stamp || "
      "obj/foo/datadep.stamp\n";
  EXPECT_EQ(expected, out.str());
}
