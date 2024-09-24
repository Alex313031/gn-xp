// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_FLATTENER_H_
#define TOOLS_GN_TARGET_FLATTENER_H_

#include <string>
#include <vector>

#include "gn/label_ptr.h"
#include "gn/unique_vector.h"

class BuildSettings;
class Err;
class FunctionCallNode;
class Scope;
class SubstitutionPattern;
class Value;

// Fills the variables in a Target object from a Scope (the result of a script
// execution). Target-type-specific derivations of this class will be used
// for each different type of function call. This class implements the common
// behavior.
class TargetFlattener  {
 public:
  TargetFlattener(Target* target, const Target* flatten_target, Err* err);
  ~TargetFlattener() = default;


  static void FlattenTarget(Target* target, Err* err);

 private:
  void Flatten();
  void FlattenTargets();

  const BuildSettings* GetBuildSettings() const;

  virtual bool FlattenSources();
  bool FlattenPublic();
  bool FlattenFriends();
  bool FlattenAllowCircularIncludesFrom();
  bool FlattenConfigValues();
  bool FlattenConfigs();
  bool FillOutputs(bool allow_substitutions);
  bool FillCheckIncludes();
  bool FillOutputExtension();

  // Rrturns true if the given pattern will expand to a file in the output
  // directory. If not, returns false and sets the error, blaming the given
  // Value.
  bool EnsureSubstitutionIsInOutputDir(const SubstitutionPattern& pattern,
                                       const Value& original_value);

  Target* target_;
  const Target* flatten_targets_;
  Err* err_;

 private:
  bool FlattenDependentConfigs();  // Includes all types of dependent configs.
  bool FlattenData();
  bool FlattenDependencies();  // Includes data dependencies.

  bool FlattenMetadata();
  bool FlattenAssertNoDeps();
  bool FlattenWriteRuntimeDeps();

  // Reads configs/deps from the given var name, and uses the given setting on
  // the target to save them.
  bool FlattenGenericConfigs(const UniqueVector<LabelConfigPair>* from,
                          UniqueVector<LabelConfigPair>* dest);
  bool FlattenGenericDeps(const LabelTargetVector* from, LabelTargetVector* dest);


  TargetFlattener(const TargetFlattener&) = delete;
  TargetFlattener& operator=(const TargetFlattener&) = delete;
};

#endif  // TOOLS_GN_TARGET_FLATTENER_H_
