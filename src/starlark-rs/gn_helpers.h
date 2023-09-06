// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_STARLARK_GN_HELPERS_H_
#define TOOLS_STARLARK_GN_HELPERS_H_

#include <cxx.h>
#include "gn/err.h"
#include "gn/scope.h"
#include "gn/token.h"
#include "gn/value.h"
#include "gn/parse_tree.h"

// Wrapper for safely allowing Rust to indirectly hold a GN scope and execute on it.
// To safely use this wrapper, the GN scope must outlive it.
class GNExecContext {
public:
    explicit GNExecContext(Scope* scope) : scope_(scope) {}
    void ExecuteFunctionCall(std::unique_ptr<FunctionCallNode> function, Err& err);
private:
    Scope* scope_;
};

// Convenience function to build a Token. This lets us:
// - Not worry about creating a InputFile pointer for now.
// - Abstract std::string_view lifetime concerns away from Rust.
Token make_token(int line_number, int column_number, Token::Type type, rust::Str value);

// Currently there's no upcast in cxx. Make a convenience helper like
// https://github.com/dtolnay/cxx/issues/797#issuecomment-809951817
inline std::unique_ptr<ParseNode> upcast_binary_op(std::unique_ptr<BinaryOpNode> node) { return node; }
inline std::unique_ptr<ParseNode> upcast_list(std::unique_ptr<ListNode> node) { return node; }
// However for making `ParseNode` let's provide convenience helpers rather than using upcast which isn't nice:
inline std::unique_ptr<ParseNode> make_identifier_node(const Token& token) { return std::make_unique<IdentifierNode>(token); }
inline std::unique_ptr<ParseNode> make_literal_node(const Token& token) { return std::make_unique<LiteralNode>(token); }

#endif  // TOOLS_STARLARK_GN_HELPERS_H_
