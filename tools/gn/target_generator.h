// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_GENERATOR_H_
#define TOOLS_GN_TARGET_GENERATOR_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "tools/gn/label_ptr.h"
#include "tools/gn/target.h"
#include "tools/gn/unique_vector.h"

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
class TargetGenerator {
 public:
  TargetGenerator(Target* target,
                  Scope* scope,
                  const FunctionCallNode* function_call,
                  Err* err);

  virtual ~TargetGenerator();

  void Run();

  // The function call is the parse tree node that invoked the target.
  // err() will be set on failure.
  static void GenerateTarget(Scope* scope,
                             const FunctionCallNode* function_call,
                             const std::vector<Value>& args,
                             const std::string& output_type,
                             Err* err);

  void Finish(Target::UnfinishedVars& unfinished_vars);

  // The function call is the parse tree node that invoked the target.
  // err() will be set on failure.
  static void FinishTarget(Target* target,
                           Target::UnfinishedVars& unfinished_vars,
                           Err* err);

 protected:
  // Derived classes implement this to do type-specific generation.
  virtual void DoRun() = 0;
  virtual void DoFinish(Target::UnfinishedVars& unfinished_vars) = 0;

  const BuildSettings* GetBuildSettings() const;

  bool FillSources();
  bool FillPublic();
  bool FillConfigs();
  bool FillOutputs(bool allow_substitutions);
  bool FillCheckIncludes();

  // Rrturns true if the given pattern will expand to a file in the output
  // directory. If not, returns false and sets the error, blaming the given
  // Value.
  bool EnsureSubstitutionIsInOutputDir(const SubstitutionPattern& pattern,
                                       const Value& original_value);

  // Wraps the given opaque value into the appropriate resolution.
  bool WrapOpaque(const std::string& variable, Value value);

  Target* target_;
  Scope* scope_;
  const FunctionCallNode* function_call_;
  Err* err_;
  bool allow_opaque_;

 private:
  bool FillDependentConfigs();  // Includes all types of dependent configs.
  bool FillData();
  bool FillDependencies();  // Includes data dependencies.
  bool FillMetadata();
  bool FillTestonly();
  bool FillAssertNoDeps();
  bool FillWriteRuntimeDeps();

  // Reads configs/deps from the given var name, and uses the given setting on
  // the target to save them.
  bool FillGenericConfigs(const char* var_name,
                          UniqueVector<LabelConfigPair>* dest);
  bool FillGenericDeps(const char* var_name, LabelTargetVector* dest);

  void disallow_opaque() { allow_opaque_ = false; }

  DISALLOW_COPY_AND_ASSIGN(TargetGenerator);
};

#endif  // TOOLS_GN_TARGET_GENERATOR_H_
