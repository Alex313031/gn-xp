// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/metadata_walk.h"

#include "tools/gn/deps_iterator.h"
#include "tools/gn/settings.h"

namespace {

std::string DirWithNoTrailingSlash(const SourceDir& dir) {
  // Be careful not to trim if the input is just "/" or "//".
  if (dir.value().size() > 2)
    return dir.value().substr(0, dir.value().size() - 1);
  return dir.value();
}

}  // namespace

std::vector<Value> WalkMetadata(
    UniqueVector<const Target*>& targets,
    const std::vector<base::StringPiece>& keys_to_extract,
    const std::vector<base::StringPiece>& keys_to_walk,
    bool rebase_files,
    Err* err) {
  std::vector<Value> result;
  std::vector<Value> next_walk_keys;
  // Since we may be modifying `targets` in the loop, use a normal loop instead
  // of a range-based one.
  for (size_t i = 0; i < targets.size(); ++i) {
    const Target* target = targets[i];
    if (!target->metadata().WalkStep(target->settings()->build_settings(),
                                     keys_to_extract, keys_to_walk,
                                     next_walk_keys, result, rebase_files, err))
      return std::vector<Value>();

    // Gather walk keys and find the appropriate target. Targets identified in
    // the walk key set must be deps or data_deps of the declaring target.
    const DepsIteratorRange& allDeps = target->GetDeps(Target::DEPS_ALL);
    for (const auto& next : next_walk_keys) {
      CHECK(next.type() == Value::STRING);
      // If we hit an empty string in this list, add all deps and data_deps.
      if (next.string_value().empty()) {
        for (const auto& dep : allDeps)
          targets.push_back(dep.ptr);
        break;
      }

      // Otherwise, look through the target's deps for the specified one.
      bool found_next = false;
      for (const auto& dep : allDeps) {
        // Match against both the label with the toolchain and the name without
        // it, to cover possible inputs.
        if (dep.label.GetUserVisibleName(true) == next.string_value() ||
            dep.label.GetUserVisibleName(false) == next.string_value() ||
            DirWithNoTrailingSlash(dep.label.dir()) == next.string_value()) {
          found_next = true;
          targets.push_back(dep.ptr);
          break;
        }
      }
      // If we didn't find the specified dep in the target, that's an error.
      // Propagate it back to the user.
      if (!found_next) {
        *err = Err(next.origin(),
                   std::string("I was expecting ") + next.string_value() +
                       std::string(" to be a dependency of ") +
                       target->label().GetUserVisibleName(false) +
                       ". Make sure it's included in the deps or data_deps.");
        return std::vector<Value>();
      }
    }

    // Clear the walk keys vector for the next iteration.
    next_walk_keys.clear();
  }

  return std::move(result);
}