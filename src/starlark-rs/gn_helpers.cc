// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cxx.h>
#include "gn/err.h"
#include "gn/scope.h"
#include "gn/token.h"
#include "gn/value.h"
#include "gn/parse_tree.h"
#include "starlark-rs/gn_helpers.h"

void GNExecContext::ExecuteFunctionCall(std::unique_ptr<FunctionCallNode> function, Err& err) {
    function->Execute(scope_, &err);
    // deliberately memory leak this function so that GN can use it later
    // (e.g. for checking where a target was referenced from)
    function.release();
    // (the proper way to do this is probably we need to store these ParseNodes somewhere outside.
    // the execcontext is not long lived enough, this probably has to be the StarlarkInputLoader)
    // (and we would want to annotate rust with lifetimes so that we can ensure executed functions
    // must outlive the entire GN execution context)
};

Token make_token(int line_number, int column_number, Token::Type type, rust::Str value) {
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
