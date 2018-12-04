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
#include "tools/gn/target.h"
#include "tools/gn/value.h"

namespace functions {

namespace {

bool ResolveLabel(Scope* scope,
                  const FunctionCallNode* function,
                  const Value& name,
                  Value* result,
                  Err* err) {
  if (!name.VerifyTypeIs(Value::STRING, err))
    return false;
  const Value* cur_target_name = scope->GetValue("target_name");
  if (!cur_target_name) {
    *(err) = Err(function->function(), "Invalid use of get_metadata.",
                 "This should only be used inside of a target.");
    return false;
  }

  Value cur_label(name.origin(), Value::STRING);
  if (name.string_value().empty())
    cur_label.string_value() = ":" + cur_target_name->string_value();
  else
    cur_label = name;

  // Resolve the requested label.
  Label label = Label::Resolve(scope->GetSourceDir(),
                               ToolchainLabelForScope(scope), cur_label, err);
  if (label.is_null())
    return false;

  *(result) = Value(name.origin(), label.GetUserVisibleName(true));
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

  std::vector<Value> collect_targets;
  for (auto& val : args[0].list_value()) {
    Value result;
    if (!ResolveLabel(scope, function, val, &result, err))
      return Value();
    collect_targets.push_back(result);
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

  Value retval(function, [=](const Target* target, Err* err) {
    Value contents(function, Value::LIST);
    for (const auto& val : collect_targets) {
      if (!val.VerifyTypeIs(Value::STRING, err))
        return Value();

      // Do this target if necessary.
      std::set<const Target*> targets_walked;
      if (val.string_value() == target->label().GetUserVisibleName(true)) {
        if (!target->GetMetadata(data_keys, walk_keys, rebase,
                                 /*deps_only = */ false, &contents.list_value(),
                                 &targets_walked, err))
          return Value();
        continue;
      }

      // Check that the specified strings are in the target's deps.
      bool found_next = false;
      for (const auto& dep : target->GetDeps(Target::DEPS_ALL)) {
        // Match against the label with the toolchain.
        if (dep.label.GetUserVisibleName(true) == val.string_value()) {
          // If we haven't walked this dep yet, go down into it.
          auto pair = targets_walked.insert(dep.ptr);
          if (pair.second) {
            if (!dep.ptr->GetMetadata(data_keys, walk_keys, rebase, false,
                                      &contents.list_value(), &targets_walked,
                                      err))
              return Value();
          }
          // We found it, so we can exit this search now.
          found_next = true;
          break;
        }
      }
      // If we didn't find the specified dep in the target, that's an error.
      // Propagate it back to the user.
      if (!found_next) {
        *err =
            Err(val.origin(),
                std::string("I was expecting ") + val.string_value() +
                    std::string(" to be a dependency of ") +
                    target->label().GetUserVisibleName(true) +
                    ". Make sure it's included in the deps or data_deps, and "
                    "that you've specified the appropriate toolchain.");
        return Value();
      }
    }
    return contents;
  });
  return retval;
}

}  // namespace functions
