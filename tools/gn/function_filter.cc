// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"

namespace functions {

const char kFilter[] = "filter";
const char kFilter_HelpShort[] =
    "filter: Remove values from a list that match a set of patterns.";
const char kFilter_Help[] =
    R"(filter: Remove values from a list that match a set of patterns.

  filter(values, patterns)

  The first argument is a list of strings and the second a list of patterns.
  The returned value is a list of strings containing all strings from values
  that matched none of the patterns.

Examples
  values = [ "foo.cc", "foo.h", "foo.proto" ]
  result = filter(values, [ "*.proto" ])
  # result will be [ "foo.h", "foo.cc" ]
)";

Value RunFilter(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                Err* err) {
  if (args.size() != 2) {
    *err = Err(function, "Expecting two arguments to filter.");
    return Value();
  }

  // Extract the "patterns".
  PatternList patterns;
  patterns.SetFromValue(args[1], err);
  if (err->has_error())
    return Value();

  if (args[0].type() != Value::LIST) {
    *err = Err(args[0], "First argument must be a list of strings.");
    return Value();
  }

  Value result(function, Value::LIST);
  for (const auto& value : args[0].list_value()) {
    if (value.type() != Value::STRING) {
      *err = Err(args[0], "First argument must be a list of strings.");
      return Value();
    }

    if (!patterns.MatchesString(value.string_value())) {
      result.list_value().push_back(value);
    }
  }
  return result;
}

}  // namespace functions
