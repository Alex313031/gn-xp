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

void GNExecContext::ExecuteTarget(std::unique_ptr<FunctionCallNode> function, Err& err) {
  function->Execute(scope_, &err);
  // deliberately memory leak this function so that GN can use it later
  // (e.g. for checking where a target was referenced from)
  function.release();
  // (the proper way to do this is probably we need to store these ParseNodes somewhere outside.
  // the execcontext is not long lived enough, this probably has to be the StarlarkInputLoader)
  // (and we would want to annotate rust with lifetimes so that we can ensure executed functions
  // must outlive the entire GN execution context)
};

// This logic is implemented in C++ because of heavy pointer usage
// (needs Scope* to execute the import, needs Scope* to merge import scope into parent scope)
std::unique_ptr<GNImportResult> GNExecContext::ExecuteImport(std::unique_ptr<FunctionCallNode> import, Err& err) {
  auto result = std::make_unique<GNImportResult>();
  auto import_scope = Scope(scope_);

  import->Execute(&import_scope, &err);
  if (err.has_error()) {
    return result;
  }

  // as with above we need to memory leak the import.
  // and fixing this also means lifetime annotations to keep imports around.
  const ParseNode* import_ptr = import.release();

  // Extract templates.
  std::vector<std::string_view> template_names;
  import_scope.GetCurrentScopeTemplateNames(&template_names);
  for (auto template_name : template_names) {
    result->template_names.push_back(std::string(template_name));
  }

  // Merge scope into current.
  // TODO: handle if false
  Scope::MergeOptions options;
  options.skip_private_vars = true;
  options.mark_dest_used = true;  // Don't require all imported values be used.
  import_scope.NonRecursiveMergeTo(scope_, options, import_ptr, "import", &err);
  return result;
}

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
