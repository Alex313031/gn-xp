// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/metadata_walk.h"

std::vector<Value> WalkMetadata(
    UniqueVector<const Target*>& targets_walked,
    const UniqueVector<const Target*>& targets_to_walk,
    const std::vector<base::StringPiece>& keys_to_extract,
    const std::vector<base::StringPiece>& keys_to_walk,
    bool rebase_files,
    Err* err) {
  std::vector<Value> result;
  for (const auto* target : targets_to_walk) {
    if (targets_walked.push_back(target)) {
      if (!target->GetMetadata(result, targets_walked, keys_to_extract, keys_to_walk,
                          rebase_files, err))
        return std::vector<Value>();
    }
  }
  return result;
}