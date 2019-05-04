// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_TARGET_GENERATOR_H_
#define TOOLS_GN_RUST_TARGET_GENERATOR_H_

#include "base/macros.h"
#include "tools/gn/target.h"
#include "tools/gn/target_generator.h"

// Collects and writes specified data.
class RustTargetGenerator : public TargetGenerator {
 public:
  RustTargetGenerator(Target* target,
                      Scope* scope,
                      const FunctionCallNode* function_call,
                      const char* type,
                      Err* err);
  ~RustTargetGenerator() override;

 protected:
  void DoRun() override;

 private:
  bool FillCrateRoot();
  bool FillCrateType();
  const char* output_type_;

  DISALLOW_COPY_AND_ASSIGN(RustTargetGenerator);
};

#endif  // TOOLS_GN_GENERATED_FILE_TARGET_GENERATOR_H_
