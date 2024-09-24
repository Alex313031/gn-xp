// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_FLATTENER_H_
#define TOOLS_GN_TARGET_FLATTENER_H_

#include <string>
#include <vector>

#include "gn/label_ptr.h"
#include "gn/unique_vector.h"
#include "gn/target.h"

class Err;

// Flattens the variables in a Target object from a Scope (the result of a script
// execution). 
class TargetFlattener  {
 public:
  TargetFlattener(Target* target, const LabelTargetPair* flatten_dep, Err* err);
  TargetFlattener(Target* target, Err* err);
  ~TargetFlattener() = default;

  static bool FlattenTarget(Target* target, Err* err);

 private:
  bool FlattenCommon();
  bool FlattenTarget();

  bool FlattenSources();
  bool FlattenPublic();
  bool FlattenFriends();
  bool FlattenAllowCircularIncludesFrom();
  bool FlattenConfigValues();
  bool FlattenAllConfigs();

  Target* target_;
  const Target* flatten_dep_;
  const LabelTargetPair* flatten_dep_pair_;
  Err* err_;

 private:
  bool FlattenData();
  bool FlattenDependencies();  // Includes data dependencies.

  bool FlattenMetadata();
  bool FlattenAssertNoDeps();

  // Reads configs/deps from the given var name, and uses the given setting on
  // the target to save them.
  bool FlattenGenericConfigs(const UniqueVector<LabelConfigPair>* from,
                          UniqueVector<LabelConfigPair>* dest);
  bool FlattenGenericDeps(const LabelTargetVector* from, LabelTargetVector* dest);


  TargetFlattener(const TargetFlattener&) = delete;
  TargetFlattener& operator=(const TargetFlattener&) = delete;
};

#endif  // TOOLS_GN_TARGET_FLATTENER_H_
