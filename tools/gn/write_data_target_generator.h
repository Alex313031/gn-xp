// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_WRITE_DATA_TARGET_GENERATOR_H_
#define TOOLS_GN_WRITE_DATA_TARGET_GENERATOR_H_

#include "base/macros.h"
#include "tools/gn/target.h"
#include "tools/gn/target_generator.h"

// Collects and writes specified data.
class WriteDataTargetGenerator : public TargetGenerator {
 public:
  WriteDataTargetGenerator(Target* target,
                           Scope* scope,
                           const FunctionCallNode* function_call,
                           Target::OutputType type,
                           Err* err);
  ~WriteDataTargetGenerator() override;

 protected:
  void DoRun() override;
  void DoFinish(Target::UnfinishedVars& unfinished_vars) override;

 private:
  bool FillWriteDataOutput();
  bool FillOutputConversion();
  bool FillRebase();
  bool FillDataKeys();
  bool FillWalkKeys();
  bool FillContents();

  bool ContentsDefined(const base::StringPiece& name);

  Target::OutputType output_type_;
  bool contents_defined_;
  bool data_keys_defined_;

  DISALLOW_COPY_AND_ASSIGN(WriteDataTargetGenerator);
};

#endif  // TOOLS_GN_WRITE_DATA_TARGET_GENERATOR_H_
