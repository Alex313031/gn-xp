// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_EXCLUDER_H_
#define TOOLS_GN_TARGET_EXCLUDER_H_

#include "gn/label_ptr.h"
#include "gn/unique_vector.h"
#include "gn/target.h"

class Err;

// Exclude the variables in a Target object.
class TargetExcluder  {
 public:
  TargetExcluder(Target* target, Err* err);
  ~TargetExcluder() = default;


  static bool ExcluderTarget(Target* target, Err* err);

 private:
  bool Excluder();
  bool ExcluderTarget();

  virtual bool ExcludeSources();
  bool ExcludePublic();
  bool ExcludeFriends();
  bool ExcludeAllowCircularIncludesFrom();
  bool ExcludeConfigValues();
  bool ExcludeConfigs();

  Target* target_;
  Err* err_;

 private:
  bool ExcludeDependentConfigs();
  bool ExcludeData();
  bool ExcludeDependencies();

  bool ExcludeMetadata();
  bool ExcludeAssertNoDeps();
  bool ExcludeWriteRuntimeDeps();

  bool ExcludeGenericConfigs(const UniqueVector<LabelConfigPair>* exclude,
                              UniqueVector<LabelConfigPair>* dest);
  bool ExcludeGenericDeps(const LabelTargetVector* exclude,
                          LabelTargetVector* dest);

  TargetExcluder(const TargetExcluder&) = delete;
  TargetExcluder& operator=(const TargetExcluder&) = delete;
};

#endif  // TOOLS_GN_TARGET_EXCLUDER_H_
