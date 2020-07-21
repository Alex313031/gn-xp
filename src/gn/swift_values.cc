// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/swift_values.h"

#include "gn/deps_iterator.h"
#include "gn/settings.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

SwiftValues::SwiftValues() = default;

SwiftValues::~SwiftValues() = default;

void SwiftValues::OnTargetResolved(const Target* target) {
  // Copy dependencies public Swift module search path to self and to re-export
  // public Swift module search path of public dependencies.
  for (const auto& pair : target->GetDeps(Target::DEPS_LINKED)) {
    if (pair.ptr->toolchain() == target->toolchain() ||
        pair.ptr->toolchain()->propagates_configs()) {
      modules_.Append(pair.ptr->swift_values().public_modules().begin(),
                      pair.ptr->swift_values().public_modules().end());
    }
  }

  for (const auto& pair : target->public_deps()) {
    if (pair.ptr->toolchain() == target->toolchain() ||
        pair.ptr->toolchain()->propagates_configs())
      public_modules_.Append(pair.ptr->swift_values().public_modules().begin(),
                             pair.ptr->swift_values().public_modules().end());
  }

  if (!target->IsBinary() || !target->source_types_used().SwiftSourceUsed())
    return;

  const Tool* tool =
      target->toolchain()->GetToolForSourceType(SourceFile::SOURCE_SWIFT);
  CHECK(tool->outputs().list().size() >= 1);

  module_output_file_ = SubstitutionWriter::ApplyPatternToSwiftAsOutputFile(
      target, tool, tool->outputs().list()[0]);

  public_modules_.push_back(target);
}
