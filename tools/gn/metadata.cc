// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/metadata.h"

void Metadata::walk(const std::vector<base::StringPiece>& keys_to_extract,
                    const std::vector<base::StringPiece>& keys_to_walk,
                    std::vector<base::StringPiece>& next_walk_keys,
                    std::vector<Value>& result,
                    Err& err,
                    bool rebase_files) const {
  // If there's no metadata, there's nothing to find, so quick exit.
  if (contents_.empty()) {
  	next_walk_keys.emplace_back("");
    return;
  }

  // Pull the data from each specified key.
  for (const auto& key : keys_to_extract) {
    auto iter = contents_.find(key);
    if (iter == contents_.end())
      break;
    if (!iter->second.VerifyTypeIs(Value::LIST, &err))
      return;
    if (rebase_files) {
      for (const auto& val : iter->second.list_value()) {
        if (!val.VerifyTypeIs(Value::STRING, &err))
          return;
        // TODO(juliehockett): Do we want to consider absolute paths here? In
        // which case we'd need to propagate the root_path_utf8 from
        // build_settings as well.
        std::string filename =
            source_dir_.ResolveRelativeAs(/*as_file = */ true, val, &err);
        if (err.has_error())
          return;
        result.emplace_back(val.origin(), filename);
      }
    } else {
      result.insert(result.end(), iter->second.list_value().begin(),
                    iter->second.list_value().end());
    }
  }

  // Get the targets to look at next. If no keys_to_walk are present, we push
  // the empty string to the list so that the target knows to include its deps
  // and data_deps. The values used here must be lists of strings.
  bool found_walk_key = false;
  for (const auto& key : keys_to_walk) {
    auto iter = contents_.find(key);
    if (iter != contents_.end()) {
      found_walk_key = true;
      if (!iter->second.VerifyTypeIs(Value::LIST, &err))
        return;
      for (const auto& val : iter->second.list_value()) {
      	if (!val.VerifyTypeIs(Value::STRING, &err))
          return;
        next_walk_keys.emplace_back(val.string_value());
      }
    }
  }

  if (!found_walk_key)
    next_walk_keys.emplace_back("");
}
