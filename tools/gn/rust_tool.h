// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_TOOL_H_
#define TOOLS_GN_RUST_TOOL_H_

#include <string>

#include "base/logging.h"
#include "base/macros.h"
#include "tools/gn/label.h"
#include "tools/gn/label_ptr.h"
#include "tools/gn/source_file_type.h"
#include "tools/gn/substitution_list.h"
#include "tools/gn/substitution_pattern.h"
#include "tools/gn/tool.h"

class RustTool : public Tool {
 public:
  // Rust tools
  static const char* kRsToolRust;
  static const char* kRsToolRustAlink;
  static const char* kRsToolRustSolink;
  static const char* kRsToolRustSolinkModule;
  static const char* kRsToolRustProcMacro;

  RustTool(const char* n);
  ~RustTool();

  // Manual RTTI and required functions ---------------------------------------

  bool InitTool(Scope* block_scope, Toolchain* toolchain, Err* err);
  bool ValidateName(const char* name) const override;
  void SetComplete() override;
  bool ValidateSubstitution(SubstitutionType sub_type) const override;

  RustTool* AsRust() override;
  const RustTool* AsRust() const override;

 private:
  bool ReadOutputsPatternList(Scope* scope,
                              const char* var,
                              SubstitutionList* field,
                              Err* err);

  DISALLOW_COPY_AND_ASSIGN(RustTool);
};

#endif  // TOOLS_GN_RUST_TOOL_H_
