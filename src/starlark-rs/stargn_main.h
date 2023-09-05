// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_STARLARK_STARGN_MAIN_H_
#define TOOLS_STARLARK_STARGN_MAIN_H_

int call_main(int argc, char** argv);

// These are here instead of starlark_glue.h, because it seems impossible to both depend on Rust types
// and also expose functions to Rust in the same header file.
// TODO: reorganize the files here because it doesn't really make sense for this declaration to live here.
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
    inline void ExecuteFunctionCall(std::unique_ptr<FunctionCallNode> function, Err& err) {
        function->Execute(scope_, &err);
        // deliberately memory leak this function so that GN can use it later
        // (e.g. for checking where a target was referenced from)
        function.release();
        // (the proper way to do this is probably we need to store these ParseNodes somewhere outside.
        // the execcontext is not long lived enough, this probably has to be the StarlarkInputLoader)
        // (and we would want to annotate rust with lifetimes so that we can ensure executed functions
        // must outlive the entire GN execution context)
    };
private:
    Scope* scope_;
};
// Convenience function to build a Token. This lets us:
// - Not worry about creating a InputFile pointer for now.
// - Abstract std::string_view lifetime concerns away from Rust.
inline Token make_token(int line_number, int column_number, Token::Type type, rust::Str value) {
    // Take the Rust &str and copy it onto the C++ heap. Token wants an std::string_view, which means
    // if we construct a std::string_view of the &str it will become a dangling reference.
    // (Even if this was a Rust String, that won't help because we take ownership then destruct it.)
    // This means memory leakage, which we consider fine because GN is short-lived and already deliberately
    // avoids freeing memory in order to finish executing more quickly (see gn_main.cc)
    // If we wanted to do this "properly", then we probably need to add a lifetime annotation in Rust
    // to indicate the Token shares the &str's lifetime: https://cxx.rs/extern-c++.html#lifetimes
    // (Or alternatively define a cxx opaque type for std::string_view: https://github.com/dtolnay/cxx/issues/734)
    std::string* token_value = new std::string(value);
    return Token(Location(NULL, line_number, column_number), type, *token_value);
}
// Currently there's no upcast in cxx. Make a convenience helper like
// https://github.com/dtolnay/cxx/issues/797#issuecomment-809951817
inline std::unique_ptr<ParseNode> upcast_binary_op(std::unique_ptr<BinaryOpNode> node) { return node; }
inline std::unique_ptr<ParseNode> upcast_list(std::unique_ptr<ListNode> node) { return node; }
// However for making `ParseNode` let's provide convenience helpers rather than using upcast which isn't nice:
inline std::unique_ptr<ParseNode> make_identifier_node(const Token& token) { return std::make_unique<IdentifierNode>(token); }
inline std::unique_ptr<ParseNode> make_literal_node(const Token& token) { return std::make_unique<LiteralNode>(token); }

#endif  // TOOLS_STARLARK_STARGN_MAIN_H_
