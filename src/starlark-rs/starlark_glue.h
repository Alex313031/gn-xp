// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_STARLARK_STARLARK_GLUE_H_
#define TOOLS_STARLARK_STARLARK_GLUE_H_

#ifdef __cplusplus

#include "cxxgen.h"
#include "cxxgen1.h"
#include "gn/build_settings.h"
#include "gn/input_alternate_loader.h"
#include "gn/input_file.h"

// These are implemented in C++ and not Rust because only autocxx is capable of subclassing,
// however both can't be handled by autocxx.
// For InputLoadResult, Scope can't be converted.
// For InputAlternateLoader, BuildSettings can't be converted.
// Until we can use an opaque type or autocxx supports converting these classes
// (see https://github.com/google/autocxx/issues/1328), use C++ for now.

class StarlarkLoadResult : public InputLoadResult {
 public:
  StarlarkLoadResult(rust::Box<ParseResult>&& parse_result) : parse_result_(std::make_shared<rust::Box<ParseResult>>(std::move(parse_result))) { }
  Value Execute(Scope *scope, Err *err) const;

 private:
  // this was originally a rust::Box&, but that didn't survive the round trip.
  // wrapping it in a shared_ptr allows it to survive the round trip
  // other usages of shared_ptr to wrap rust::Box in the wild:
  // https://github.com/isgasho/cxx-async/blob/4b83b3ec0893a3f66fd331074bf45d23d9a07e72/cxx-async/include/rust/cxx_async.h#L318
  std::shared_ptr<rust::Box<ParseResult>> parse_result_;
};

class StarlarkInputLoader : public InputAlternateLoader {
 public:
  StarlarkInputLoader(const BuildSettings* build_settings) : build_settings_(build_settings) { }
  std::unique_ptr<InputLoadResult> TryLoadAlternateFor(const SourceFile& source_file, InputFile *input_file) const;

 private:
  const BuildSettings* build_settings_;
};

#endif

#endif  // TOOLS_STARLARK_STARLARK_GLUE_H_