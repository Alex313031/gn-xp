// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <stdbool.h>
#include <stdio.h>
#include "_cgo_export.h"
#include "starlark_glue.h"

#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/input_alternate_loader.h"
#include "gn/parse_tree.h"

Value StarlarkLoadResult::Execute(Scope *scope, Err *err) const {
  // TODO: error handling?
  ExecStarlark(scope, go_starlark_program_handle_);
  return Value();
}

std::unique_ptr<InputLoadResult> StarlarkInputLoader::TryLoadAlternateFor(const SourceFile& source_file, InputFile *input_file) const {
  base::FilePath starlark_input_path =
      build_settings_->GetFullPath(source_file).ReplaceExtension(".stargn");
  if (!input_file->Load(starlark_input_path)) {
    // TODO: return Err maybe?
    return NULL;
  }
  // Need non-const char* to pass to Go, so copy the strings as non-const.
  std::string source = input_file->contents();
  std::string filename = starlark_input_path.value();

  // Store handle to parsed Starlark from Go.
  uint32_t go_starlark_program_handle = ParseStarlark(input_file, source.data(), filename.data());
  std::unique_ptr<StarlarkLoadResult> ptr = std::make_unique<StarlarkLoadResult>(StarlarkLoadResult(go_starlark_program_handle));
  return std::unique_ptr<InputLoadResult>(ptr.release());
}

GNImportResult* run_gn_import(void* scope_ptr, const char* filename_quoted) {
  Scope* scope = static_cast<Scope*>(scope_ptr);
  Scope import_scope = Scope(scope);

  std::unique_ptr<ListNode> args = std::make_unique<ListNode>();
  args->append_item(std::make_unique<LiteralNode>(Token(Location(NULL, 42, 42), Token::STRING, filename_quoted)));

  FunctionCallNode* function = new FunctionCallNode();
  function->set_function(Token(Location(NULL, 42, 42), Token::IDENTIFIER, functions::kImport));
  function->set_args(std::move(args));

  Err err;
  function->Execute(&import_scope, &err);

  if (err.has_error()) {
    std::cout << "ERROR! " << err.message() << " " << err.help_text() << std::endl;
  }

  std::vector<std::string_view> template_names;
  import_scope.GetCurrentScopeTemplateNames(&template_names);

  // Convert to C result.
  GNImportResult* result = (GNImportResult*)malloc(sizeof(*result));
  result->template_count = template_names.size();
  result->template_names = (const char**)malloc(template_names.size() * sizeof(char*));
  for (auto it = template_names.begin(); it != template_names.end(); ++it) {
    int i = std::distance(template_names.begin(), it);
    result->template_names[i] = (new std::string(*it))->c_str();
  }

  // Merge scope into current.
  // TODO: handle if false
  Scope::MergeOptions options;
  options.skip_private_vars = true;
  options.mark_dest_used = true;  // Don't require all imported values be used.
  import_scope.NonRecursiveMergeTo(scope, options, function, "import", &err);
  return result;
}

void* run_gn_function(void* scope_ptr, const char* function_identifier, const char* arg0_quoted, void* block_ptr) {
  Scope* scope = static_cast<Scope*>(scope_ptr);
  BlockNode* block = static_cast<BlockNode*>(block_ptr);
  block->set_begin_token(Token(Location(NULL, 42, 42), Token::LEFT_BRACE, "{"));
  block->set_end(std::make_unique<EndNode>(Token(Location(NULL, 42, 42), Token::LEFT_BRACE, "}")));
  std::unique_ptr<ListNode> args = std::make_unique<ListNode>();
  args->append_item(std::make_unique<LiteralNode>(Token(Location(NULL, 42, 42), Token::STRING, arg0_quoted)));


  FunctionCallNode* function = new FunctionCallNode();
  function->set_function(Token(Location(NULL, 42, 42), Token::IDENTIFIER, function_identifier));
  function->set_args(std::move(args));
  function->set_block(std::unique_ptr<BlockNode>(block));

  Err err;
  function->Execute(scope, &err);

  if (err.has_error()) {
    std::cout << "ERROR EXECUTING " << function_identifier << "! " << err.message() << " " << err.help_text() << std::endl;
  }

  return function; // TODO: do something with this in go land
}

void* create_block_node() {
  return new BlockNode(BlockNode::DISCARDS_RESULT);
}

void delete_block_node(void* block_ptr) {
  BlockNode* block = static_cast<BlockNode*>(block_ptr);
  delete block;
}

void append_bool_assign_to_block_node(void* block_ptr, const char* identifier, int boolean) {
  BlockNode* block = static_cast<BlockNode*>(block_ptr);
  std::unique_ptr<BinaryOpNode> binary_op = std::make_unique<BinaryOpNode>();
  binary_op->set_op(Token(Location(NULL, 42, 42), Token::EQUAL, "="));
  binary_op->set_left(std::make_unique<IdentifierNode>(Token(Location(NULL, 42, 42), Token::IDENTIFIER, identifier)));
  if (boolean) {
    binary_op->set_right(std::make_unique<LiteralNode>(Token(Location(NULL, 42, 42), Token::TRUE_TOKEN, "true")));
  } else {
    binary_op->set_right(std::make_unique<LiteralNode>(Token(Location(NULL, 42, 42), Token::FALSE_TOKEN, "false")));
  }
  block->append_statement(std::move(binary_op));
}

void append_quoted_string_assign_to_block_node(void* block_ptr, const char* identifier, const char* string) {
  BlockNode* block = static_cast<BlockNode*>(block_ptr);
  std::unique_ptr<BinaryOpNode> binary_op = std::make_unique<BinaryOpNode>();
  binary_op->set_op(Token(Location(NULL, 42, 42), Token::EQUAL, "="));
  binary_op->set_left(std::make_unique<IdentifierNode>(Token(Location(NULL, 42, 42), Token::IDENTIFIER, identifier)));
  binary_op->set_right(std::make_unique<LiteralNode>(Token(Location(NULL, 42, 42), Token::STRING, string)));
  block->append_statement(std::move(binary_op));
}

void append_list_assign_to_block_node(void* block_ptr, const char* identifier, void* list_ptr) {
  BlockNode* block = static_cast<BlockNode*>(block_ptr);
  ListNode* list = static_cast<ListNode*>(list_ptr);
  std::unique_ptr<BinaryOpNode> binary_op = std::make_unique<BinaryOpNode>();
  binary_op->set_op(Token(Location(NULL, 42, 42), Token::EQUAL, "="));
  binary_op->set_left(std::make_unique<IdentifierNode>(Token(Location(NULL, 42, 42), Token::IDENTIFIER, identifier)));
  binary_op->set_right(std::unique_ptr<ListNode>(list));
  block->append_statement(std::move(binary_op));
}

void* create_list_node() {
  return new ListNode();
}

void delete_list_node(void* list_ptr) {
  ListNode* list = static_cast<ListNode*>(list_ptr);
  delete list;
}

void append_quoted_string_to_list_node(void* list_ptr, const char* string) {
  ListNode* list = static_cast<ListNode*>(list_ptr);
  list->append_item(std::make_unique<LiteralNode>(Token(Location(NULL, 42, 42), Token::STRING, string)));
}
