// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "tools/gn/deps_iterator.h"
#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/label.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/value.h"

#include "tools/gn/standard_out.h"
namespace functions {

namespace {

bool ResolveLabel(Scope* scope,
                  const FunctionCallNode* function,
                  const Value& name,
                  Label* result,
                  Err* err) {
  if (!name.VerifyTypeIs(Value::STRING, err))
    return false;
  const Value* cur_target_name = scope->GetValue("target_name");
  if (!cur_target_name) {
    *(err) = Err(function->function(), "Invalid use of get_metadata.",
                 "This should only be used inside of a target.");
    return false;
  }

  // Resolve the requested label.
  Label label = Label::Resolve(scope->GetSourceDir(),
                               ToolchainLabelForScope(scope), name, err);
  if (label.is_null())
    return false;

  *(result) = label;
  return true;
}

}  // namespace

const char kGetMetadata[] = "get_metadata";
const char kGetMetadata_HelpShort[] = "get_metadata: ";
const char kGetMetadata_Help[] =
    R"(get_metadata: 

  get_metadata(targets,
               data_keys,
               walk_keys = [],
               rebase = false)

  TODO(juliehockett): write help.
)";

Value RunGetMetadata(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     Err* err) {
  if (args.size() < 2 || args.size() > 4) {
    *err =
        Err(function->function(), "Wrong number of arguments to get_metadata",
            "I expected between two and four arguments.");
    return Value();
  }

  // Verify and resolve labels.
  if (!args[0].VerifyTypeIs(Value::LIST, err))
    return Value();

  // If this value is the empty list, call it out and bail.
  if (args[0].list_value().empty()) {
    *err = Err(function->function(), "No targets set for get_metadata",
               "I expected at least one target to be set (or [\"\"] for the "
               "current target).");
    return Value();
  }

  std::vector<Label> collected_labels;
  for (auto& val : args[0].list_value()) {
    Label result;
    if (!ResolveLabel(scope, function, val, &result, err))
      return Value();
    collected_labels.push_back(result);
  }

  // Verify data keys. Keys must be lists of strings.
  if (!args[1].VerifyTypeIs(Value::LIST, err))
    return Value();
  // If no data keys are set, call it out and bail.
  if (args[1].list_value().empty()) {
    *err = Err(function->function(), "No data keys set for get_metadata",
               "I expected at least one data_kay to be set, because otherwise "
               "I'll do a lot of work with no results.");
    return Value();
  }

  std::vector<std::string> data_keys;
  for (const auto& val : args[1].list_value()) {
    if (!val.VerifyTypeIs(Value::STRING, err))
      return Value();
    data_keys.push_back(val.string_value());
  }

  // Verify walk keys, and set defaults if necessary. Keys must be lists of
  // strings.
  std::vector<std::string> walk_keys;
  if (args.size() > 2) {
    if (!args[2].VerifyTypeIs(Value::LIST, err))
      return Value();
    for (const auto& val : args[2].list_value()) {
      if (!val.VerifyTypeIs(Value::STRING, err))
        return Value();
      walk_keys.push_back(val.string_value());
    }
  }

  // Set rebase if necessary.
  bool rebase = false;
  if (args.size() > 3) {
    if (!args[3].VerifyTypeIs(Value::BOOLEAN, err))
      return Value();
    rebase = args[3].boolean_value();
  }

  std::set<const Target*> targets_walked;
  Value contents(function, Value::LIST);

  // These are resolved targets, registered into the resolver. This will block
  // until the specified targets are resolved, thus the circular checking.
  for (const auto& label : collected_labels) {
    const Target* current = g_scheduler->resolver().Get(label, err);
    current->GetMetadata(data_keys, walk_keys, rebase, false,
                         &contents.list_value(), &targets_walked, err);
  }

  return contents;
}

}  // namespace functions
