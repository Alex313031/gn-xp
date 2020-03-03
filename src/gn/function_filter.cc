// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/parse_tree.h"
#include "gn/scope.h"
#include "gn/value.h"

namespace functions {

const char kFilter[] = "filter";
const char kFilter_HelpShort[] =
    "filter: Remove values from a list that match a set of patterns.";
const char kFilter_Help[] =
    R"(filter: Remove values from a list that match a set of patterns.

  filter(values, exclude_patterns)
  filter(values, exclude_patterns, include_patterns)

  The argument values must be a list of strings.

  The argument exclude_patterns must be a list of patterns. All elements
  in values matching any of those patterns will be removed from the list
  that is returned by the filter function.

  The argument include_patterns, if specified, must be a list of patterns.
  Any elements in values matchin any of those patterns will be included,
  even if they match a pattern in exclude_patterns.

Examples
  values = [ "foo.cc", "foo.h", "foo.proto" ]
  result = filter(values, [ "*.proto" ])
  # result will be [ "foo.h", "foo.cc" ]

  values = [ "foo.cc", "foo.h", "foo.proto" ]
  result = filter(values, [ "*" ], [ "*.proto" ])
  # result will be [ "foo.proto" ]
)";

Value RunFilter(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err) {
  if (args.size() != 2 && args.size() != 3) {
    *err = Err(function, "Expecting two or three arguments to filter.");
    return Value();
  }

  // Extract "values".
  if (args[0].type() != Value::LIST) {
    *err = Err(args[0], "First argument must be a list of strings.");
    return Value();
  }

  // Extract "exclude_patterns".
  PatternList exclude_patterns;
  exclude_patterns.SetFromValue(args[1], err);
  if (err->has_error())
    return Value();

  // Extract "include_patterns" if specified.
  PatternList include_patterns;
  if (args.size() == 3) {
    include_patterns.SetFromValue(args[2], err);
    if (err->has_error())
      return Value();
  }

  Value result(function, Value::LIST);
  for (const auto& value : args[0].list_value()) {
    if (value.type() != Value::STRING) {
      *err = Err(args[0], "First argument must be a list of strings.");
      return Value();
    }

    if (exclude_patterns.MatchesString(value.string_value())) {
      if (!include_patterns.MatchesString(value.string_value())) {
        continue;
      }
    }

    result.list_value().push_back(value);
  }
  return result;
}

}  // namespace functions
