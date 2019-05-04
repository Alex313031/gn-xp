// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_RUST_FUNCTIONS_H_
#define TOOLS_GN_RUST_FUNCTIONS_H_

#include <map>
#include <string>
#include <vector>

#include "tools/gn/functions.h"
#include "base/strings/string_piece.h"

class Err;
class BlockNode;
class FunctionCallNode;
class Scope;
class Value;

// -----------------------------------------------------------------------------

namespace functions {

extern const char kRustExecutable[];
extern const char kRustExecutable_HelpShort[];
extern const char kRustExecutable_Help[];
Value RunRustExecutable(Scope* scope,
                        const FunctionCallNode* function,
                        const std::vector<Value>& args,
                        BlockNode* block,
                        Err* err);

extern const char kRustLibrary[];
extern const char kRustLibrary_HelpShort[];
extern const char kRustLibrary_Help[];
Value RunRustLibrary(Scope* scope,
                     const FunctionCallNode* function,
                     const std::vector<Value>& args,
                     BlockNode* block,
                     Err* err);

void InsertRustFunctions(FunctionInfoMap* info_map);

}  // namespace functions

#endif  // TOOLS_GN_RUST_FUNCTIONS_H_