// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "gn/build_settings.h"
#include "gn/err.h"
#include "gn/functions.h"
#include "gn/label_pattern.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/settings.h"
#include "gn/value.h"

namespace functions {

const char kLabelMatches[] = "label_matches";
const char kLabelMatches_HelpShort[] =
    "filter_exclude: Remove values that match a set of patterns.";
const char kLabelMatches_Help[] =
    R"(label_matches: Returns true if the label matches one of a set of patterns.

  label_matches(target_label, patterns)

  The argument patterns must be a list of label patterns (see
  "gn help label_pattern"). If the target_label matches any of the patterns,
  the function returns the value true.

Examples
  result = label_matches("//baz:bar", [ "//foo/bar/*", "//baz:*" ])
  # result will be true
)";

Value RunLabelMatches(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Expecting exactly two arguments.");
    return Value();
  }

  // Extract "values".
  if (args[0].type() != Value::STRING) {
    *err = Err(args[0], "First argument must be a target label.");
    return Value();
  }

  // Extract "patterns".
  std::vector<LabelPattern> patterns;
  for (const auto& value : args[1].list_value()) {
    if (value.type() != Value::STRING) {
      *err = Err(args[1], "Second argument must be a list of label patterns");
      return Value();
    }
    LabelPattern pattern = LabelPattern::GetPattern(
        scope->GetSourceDir(),
        scope->settings()->build_settings()->root_path_utf8(), value, err);
    if (err->has_error()) {
      return Value();
    }
    patterns.push_back(pattern);
  }

  Value result(function, Value::BOOLEAN);
  Label label =
      Label::Resolve(scope->GetSourceDir(),
                     scope->settings()->build_settings()->root_path_utf8(),
                     ToolchainLabelForScope(scope), args[0], err);
  if (label.is_null()) {
    return Value();
  }

  const bool matches_pattern = LabelPattern::VectorMatches(patterns, label);
  if (matches_pattern) {
    result.boolean_value() = true;
  }
  return result;
}

const char kFilterLabels[] = "filter_labels";
const char kFilterLabels_HelpShort[] =
    "filter_labels: Remove labels that do not match a set of patterns.";
const char kFilterLabels_Help[] =
    R"(filter_labels: Remove labels that do not match a set of patterns.

  filter_labels(labels, include_patterns)

  The argument labels must be a list of strings.

  The argument include_patterns must be a list of label patterns (see
  "gn help label_pattern"). Only elements from labels matching at least
  one of the patterns will be included.

Examples
  labels = [ "//foo:baz", "//foo/bar:baz", "//bar:baz" ]
  result = filter_labels(labels, [ "//foo:*" ])
  # result will be [ "//foo:baz" ]
)";

Value RunFilterLabels(Scope* scope,
                      const FunctionCallNode* function,
                      const std::vector<Value>& args,
                      Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Expecting exactly two arguments.");
    return Value();
  }

  // Extract "values".
  if (args[0].type() != Value::LIST) {
    *err = Err(args[0], "First argument must be a list of target labels.");
    return Value();
  }

  // Extract "patterns".
  std::vector<LabelPattern> patterns;
  for (const auto& value : args[1].list_value()) {
    if (value.type() != Value::STRING) {
      *err = Err(args[1], "Second argument must be a list of label patterns");
      return Value();
    }
    LabelPattern pattern = LabelPattern::GetPattern(
        scope->GetSourceDir(),
        scope->settings()->build_settings()->root_path_utf8(), value, err);
    if (err->has_error()) {
      return Value();
    }
    patterns.push_back(pattern);
  }

  Value result(function, Value::LIST);
  for (const auto& value : args[0].list_value()) {
    Label label =
        Label::Resolve(scope->GetSourceDir(),
                       scope->settings()->build_settings()->root_path_utf8(),
                       ToolchainLabelForScope(scope), value, err);
    if (label.is_null())
      return Value();
    const bool matches_pattern = LabelPattern::VectorMatches(patterns, label);
    if (matches_pattern) {
      result.list_value().push_back(value);
    }
  }
  return result;
}

}  // namespace functions
