// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/label.h"
#include "gn/parse_tree.h"
#include "gn/value.h"

namespace functions {

namespace {

Value GetName(const FunctionCallNode* function, const Label& label) {
  return Value(function, label.name());
}

Value GetDir(const FunctionCallNode* function, const Label& label) {
  return Value(function, DirectoryWithNoLastSlash(label.dir()));
}

Value GetTargetGenDir(const FunctionCallNode* function,
                      const Scope* scope,
                      const Label& label) {
  return Value(function, DirectoryWithNoLastSlash(GetSubBuildDirAsSourceDir(
    BuildDirContext(scope, label.GetToolchainLabel()), label.dir(),
    BuildDirType::GEN)));
}

Value GetRootGenDir(const FunctionCallNode* function,
                    const Scope* scope,
                    const Label& label) {
  return Value(function, DirectoryWithNoLastSlash(GetBuildDirAsSourceDir(
    BuildDirContext(scope, label.GetToolchainLabel()), BuildDirType::GEN)));
}

Value GetTargetOutDir(const FunctionCallNode* function,
                      const Scope* scope,
                      const Label& label) {
  return Value(function, DirectoryWithNoLastSlash(GetSubBuildDirAsSourceDir(
    BuildDirContext(scope, label.GetToolchainLabel()), label.dir(),
    BuildDirType::OBJ)));
}

Value GetRootOutDir(const FunctionCallNode* function,
                    const Scope* scope,
                    const Label& label) {
  return Value(function, DirectoryWithNoLastSlash(GetBuildDirAsSourceDir(
    BuildDirContext(scope, label.GetToolchainLabel()),
    BuildDirType::TOOLCHAIN_ROOT)));
}

Value GetToolchain(const FunctionCallNode* function, const Label& label) {
  return Value(function, label.GetToolchainLabel().GetUserVisibleName(false));
}

Value GetLabelNoToolchain(const FunctionCallNode* function, const Label& label) {
  return Value(function, label.GetWithNoToolchain().GetUserVisibleName(false));
}

Value GetLabelWithToolchain(const FunctionCallNode* function, const Label& label) {
  return Value(function, label.GetUserVisibleName(true));
}

}

const char kGetLabelInfo[] = "get_label_info";
const char kGetLabelInfo_HelpShort[] =
    "get_label_info: Get an attribute from a target's label.";
const char kGetLabelInfo_Help[] =
    R"*(get_label_info: Get an attribute from a target's label.

  get_label_info(target_label, what)

  Given the label of a target, returns some attribute of that target. The
  target need not have been previously defined in the same file, since none of
  the attributes depend on the actual target definition, only the label itself.

  See also "gn help get_target_outputs".

Possible values for the "what" parameter

  "name"
      The short name of the target. This will match the value of the
      "target_name" variable inside that target's declaration. For the label
      "//foo/bar:baz" this will return "baz".

  "dir"
      The directory containing the target's definition, with no slash at the
      end. For the label "//foo/bar:baz" this will return "//foo/bar".

  "target_gen_dir"
      The generated file directory for the target. This will match the value of
      the "target_gen_dir" variable when inside that target's declaration.

  "root_gen_dir"
      The root of the generated file tree for the target. This will match the
      value of the "root_gen_dir" variable when inside that target's
      declaration.

  "target_out_dir"
      The output directory for the target. This will match the value of the
      "target_out_dir" variable when inside that target's declaration.

  "root_out_dir"
      The root of the output file tree for the target. This will match the
      value of the "root_out_dir" variable when inside that target's
      declaration.

  "label_no_toolchain"
      The fully qualified version of this label, not including the toolchain.
      For the input ":bar" it might return "//foo:bar".

  "label_with_toolchain"
      The fully qualified version of this label, including the toolchain. For
      the input ":bar" it might return "//foo:bar(//toolchain:x64)".

  "toolchain"
      The label of the toolchain. This will match the value of the
      "current_toolchain" variable when inside that target's declaration.

  "all"
      All of the above values, named by keys matching the "what" parameters,
      returned in a scope object.

Examples

  get_label_info(":foo", "name")
  # Returns string "foo".

  get_label_info("//foo/bar:baz", "target_gen_dir")
  # Returns string "//out/Debug/gen/foo/bar".

  parts = get_label_info("//foo/bar:baz(//some:toolchain)", "all")
  # Returns { "name": "baz", "dir": "//foo/bar", ... }
)*";

Value RunGetLabelInfo(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Expected two arguments.");
    return Value();
  }

  // Resolve the requested label.
  Label label =
      Label::Resolve(scope->GetSourceDir(),
                     scope->settings()->build_settings()->root_path_utf8(),
                     ToolchainLabelForScope(scope), args[0], err);
  if (label.is_null())
    return Value();

  // Extract the "what" parameter.
  if (!args[1].VerifyTypeIs(Value::STRING, err))
    return Value();
  const std::string& what = args[1].string_value();

  if (what == "name") {
    return GetName(function, label);

  } else if (what == "dir") {
    return GetDir(function, label);

  } else if (what == "target_gen_dir") {
    return GetTargetGenDir(function, scope, label);

  } else if (what == "root_gen_dir") {
    return GetRootGenDir(function, scope, label);

  } else if (what == "target_out_dir") {
    return GetTargetOutDir(function, scope, label);

  } else if (what == "root_out_dir") {
    return GetRootOutDir(function, scope, label);

  } else if (what == "toolchain") {
    return GetToolchain(function, label);

  } else if (what == "label_no_toolchain") {
    return GetLabelNoToolchain(function, label);

  } else if (what == "label_with_toolchain") {
    return GetLabelWithToolchain(function, label);

  } else if (what == "all") {
    auto s = std::make_unique<Scope>(scope->settings());
    s->SetValue("name", GetName(function, label), function);
    s->SetValue("dir", GetDir(function, label), function);
    s->SetValue("target_gen_dir", GetTargetGenDir(function, scope, label),
      function);
    s->SetValue("root_gen_dir", GetRootGenDir(function, scope, label), function);
    s->SetValue("target_out_dir", GetTargetOutDir(function, scope, label),
      function);
    s->SetValue("root_out_dir", GetRootOutDir(function, scope, label), function);
    s->SetValue("toolchain", GetToolchain(function, label), function);
    s->SetValue("label_no_toolchain", GetLabelNoToolchain(function, label),
      function);
    s->SetValue("label_with_toolchain", GetLabelWithToolchain(function, label),
      function);
    return Value(function, std::move(s));

  } else {
    *err = Err(args[1], "Unknown value for \"what\" parameter.");
    return Value();
  }
}

}  // namespace functions
