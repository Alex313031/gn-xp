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

// Allow Value::Type to be referenced from Rust. (https://github.com/dtolnay/cxx/issues/994#issuecomment-1003901804)
using Value_Type = Value::Type;

// Results from performing a GN import.
// TODO: report imported variable names.
typedef struct GNImportResult {
  // NOTE: Not std::string_view because unsupported by cxx.
  // https://github.com/dtolnay/cxx/issues/734
  std::vector<std::string> template_names;
  // autocxx does not automatically synthesize getters for structs.
  // Maybe rewrite this as a Rust struct exported to C++. We could rely on rust::Str too
  // https://cxx.rs/binding/str.html#:~:text=Str(const%20char%20*%2C%20size_t)%3B
  inline const std::vector<std::string>& get_template_names() const { return template_names; }
} GNImportResult;

// Wrapper for safely returning a Value* from Scope#GetValue to Rust.
class GNValueResult {
 public:
  explicit GNValueResult(const Value* value) : value_(value) {}
  inline bool has_value() const { return value_ != nullptr; }
  inline const Value& get_value() const { DCHECK(value_ != nullptr); return *value_; }
 private:
  const Value* value_;
};

// Wrapper for safely allowing Rust to indirectly hold a GN scope and execute on it.
// To safely use this wrapper, the GN scope must outlive it.
class GNExecContext {
public:
    explicit GNExecContext(Scope* scope) : scope_(scope) {}
    // Execute a GN import in the current scope.
    std::unique_ptr<GNImportResult> ExecuteImport(std::unique_ptr<FunctionCallNode> import, Err& err);
    // Executes a GN target in the current scope.
    void ExecuteTarget(std::unique_ptr<FunctionCallNode> function, Err& err);
    // Safe wrapper around Scope#GetValue.
    // (This is NOT Scope#GetValueWithScope which is required for logic similar to IdentifierNode#Execute
    // to ensure that variables are not read from the same declare_args block they are defined in.)
    std::unique_ptr<GNValueResult> GetValue(rust::Str ident, bool counts_as_used) const;
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
