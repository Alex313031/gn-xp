// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_STARLARK_STARLARK_GLUE_H_
#define TOOLS_STARLARK_STARLARK_GLUE_H_

#ifdef __cplusplus

#include "gn/build_settings.h"
#include "gn/input_alternate_loader.h"
#include "gn/input_file.h"
#include "gn/value.h"

class StarlarkLoadResult : public InputLoadResult {
 public:
  StarlarkLoadResult(uint32_t go_starlark_program_handle) : go_starlark_program_handle_(go_starlark_program_handle) { }
  Value Execute(Scope *scope, Err *err) const;

 private:
  const uint32_t go_starlark_program_handle_;
};

class StarlarkInputLoader : public InputAlternateLoader {
 public:
  StarlarkInputLoader(const BuildSettings* build_settings) : build_settings_(build_settings) { }
  std::unique_ptr<InputLoadResult> TryLoadAlternateFor(const SourceFile& source_file, InputFile *input_file) const;

 private:
  const BuildSettings* build_settings_;
};

extern "C" {
#endif

typedef struct GNImportResult {
  size_t template_count;
  const char **template_names;
} GNImportResult;

int callMain(int argc, char** argv);

GNImportResult* run_gn_import(void* scope, const char* filename);
void* run_gn_function(void* scope, const char* function_identifier, const char* arg0_quoted, void* block_ptr);

void* create_block_node();
void delete_block_node(void* block_ptr);
void append_bool_assign_to_block_node(void* block_ptr, const char* identifier, int boolean);
void append_quoted_string_assign_to_block_node(void* block_ptr, const char* identifier, const char* string);
void append_list_assign_to_block_node(void* block_ptr, const char* identifier, void* list_ptr);

void* create_list_node();
void delete_list_node(void* list_ptr);
void append_quoted_string_to_list_node(void* list_ptr, const char* string);

#ifdef __cplusplus
}
#endif

#endif  // TOOLS_STARLARK_STARLARK_GLUE_H_