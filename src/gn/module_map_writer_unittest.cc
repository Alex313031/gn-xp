// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/module_map_writer.h"

#include <memory>
#include <utility>

#include "gn/parse_tree.h"
#include "gn/test_with_scope.h"
#include "gn/value.h"
#include "util/test/test.h"


TEST(ModuleMapWriter, WriteFile) {
  TestWithScope setup;
  Err err;

  Target target_bar(setup.settings(), Label(SourceDir("//bar/"), "bar"));
  target_bar.set_output_type(Target::STATIC_LIBRARY);
  target_bar.sources().push_back(SourceFile("//bar/bar.cc"));
  target_bar.public_headers().push_back(SourceFile("//bar/bar.h"));
  target_bar.set_all_headers_public(false);

  Target target_foo(setup.settings(), Label(SourceDir("//foo/"), "foo"));
  target_foo.set_output_type(Target::STATIC_LIBRARY);
  target_foo.sources().push_back(SourceFile("//foo/foo.cc"));
  target_foo.sources().push_back(SourceFile("//foo/foo_internal.h"));
  target_foo.public_headers().push_back(SourceFile("//foo/foo.h"));
  target_foo.set_all_headers_public(false);
  target_foo.private_deps().push_back(LabelPtrPair<Target>(&target_bar));

  {
    std::ostringstream module_map_out;
    ModuleMapWriter writer(&target_foo, module_map_out);

    writer.Run();

    const char expected_module_map_foo[] =
        "module \"//foo:foo\" {\n"
        "  export *\n"
        "  module \"foo.h\" {\n"
        "    export *\n"
        "    header \"../../../../foo/foo.h\"\n"
        "  }\n"
        "  module \"foo_internal.h\" {\n"
        "    export *\n"
        "    private header \"../../../../foo/foo_internal.h\"\n"
        "  }\n"
        "  use \"//bar:bar\"\n"
        "}\n";
    std::string out_str = module_map_out.str();
    EXPECT_EQ(expected_module_map_foo, out_str) << out_str;
  }

  {
    std::ostringstream module_map_out;
    ModuleMapWriter writer(&target_foo, module_map_out);

    writer.set_generate_submodules(false);
    writer.Run();

    const char expected_module_map_foo[] =
        "module \"//foo:foo\" {\n"
        "  export *\n"
        "  header \"../../../../foo/foo.h\"\n"
        "  private header \"../../../../foo/foo_internal.h\"\n"
        "  use \"//bar:bar\"\n"
        "}\n";
    std::string out_str = module_map_out.str();
    EXPECT_EQ(expected_module_map_foo, out_str) << out_str;
  }

  {
    std::ostringstream module_map_out;
    ModuleMapWriter writer(&target_foo, module_map_out);

    writer.set_extern_dependencies(true);
    writer.Run();

    const char expected_module_map_foo[] =
        "module \"//foo:foo\" {\n"
        "  export *\n"
        "  module \"foo.h\" {\n"
        "    export *\n"
        "    header \"../../../../foo/foo.h\"\n"
        "  }\n"
        "  module \"foo_internal.h\" {\n"
        "    export *\n"
        "    private header \"../../../../foo/foo_internal.h\"\n"
        "  }\n"
        "  use \"//bar:bar\"\n"
        "}\n"
        "extern module \"//bar:bar\" \"../../../../out/Debug/obj/bar/bar.cppmap\"\n";
    std::string out_str = module_map_out.str();
    EXPECT_EQ(expected_module_map_foo, out_str) << out_str;
  }

  {
    std::ostringstream module_map_out;
    ModuleMapWriter writer(&target_foo, module_map_out, true);

    writer.Run();

    const char expected_module_map_foo[] =
        "module \"//foo:foo\" {\n"
        "  export *\n"
        "  module \"foo.h\" {\n"
        "    export *\n"
        "    header \"../../foo/foo.h\"\n"
        "  }\n"
        "  module \"foo_internal.h\" {\n"
        "    export *\n"
        "    private header \"../../foo/foo_internal.h\"\n"
        "  }\n"
        "  use \"//bar:bar\"\n"
        "}\n";
    std::string out_str = module_map_out.str();
    EXPECT_EQ(expected_module_map_foo, out_str) << out_str;
  }
}
