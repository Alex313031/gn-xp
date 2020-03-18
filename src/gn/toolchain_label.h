// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TOOLCHAIN_LABEL_H_
#define TOOLS_GN_TOOLCHAIN_LABEL_H_

#include <stddef.h>

#include "gn/source_dir.h"
#include "gn/string_atom.h"

// A ToolchainLabel represents a unique toolchain label string,
// which should be in one of the following formats:
//
//    <empty>
//    /<dir>:<name>
//    //<dir>:<name>
//
// Where <empty> means, the <empty> string, <dir> is a directory path,
// possibly including sub-directories, but must not end with a separator,
// and <name> is a toolchain name that cannot include any directory separator.
// None of <dir> or <name> should include a colon.
//
class ToolchainLabel {
 public:
  // Default constructor.
  ToolchainLabel() = default;

  // Build ToolchainLabel from a |dir| and |name|. Also aborts in case of
  // invalid format.
  ToolchainLabel(const SourceDir& dir, const std::string_view name);

  // Return true iff the label is empty.
  bool empty() const { return value_.empty(); }

  // Return the label as a string.
  const std::string& str() const { return value_.str(); }

  // Return the directory and name parts of the ToolchainLabel.
  SourceDir dir() const;
  std::string_view name() const;

  // Return the build output directory for this ToolchainLabel.
  std::string GetOutputDir() const;

  size_t hash() const { return value_.hash(); }

  bool operator==(const ToolchainLabel& other) const {
    return value_.SameAs(other.value_);
  }

  bool operator!=(const ToolchainLabel& other) const {
    return !(*this == other);
  }

  bool operator<(const ToolchainLabel& other) const {
    return value_ < other.value_;
  }

 private:
  StringAtom value_;
};

#endif  // TOOLS_GN_TOOLCHAIN_LABEL_H_