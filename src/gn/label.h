// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_LABEL_H_
#define TOOLS_GN_LABEL_H_

#include <stddef.h>

#include "gn/source_dir.h"
#include "gn/string_atom.h"
#include "gn/toolchain_label.h"

class Err;
class Value;

// A label represents the name of a target or some other named thing in
// the source path. The label is always absolute and always includes a name
// part, so it starts with a slash, and has one colon.
class Label {
 public:
  Label();

  // Makes a label given an already-separated out path and name.
  // See also Resolve().
  Label(const SourceDir& dir,
        const std::string_view& name,
        const ToolchainLabel toolchain);

  // Makes a label with an empty toolchain.
  Label(const SourceDir& dir, const std::string_view& name);

  // Make a label from a toolchain label.
  explicit Label(ToolchainLabel toolchain);

  // Resolves a string from a build file that may be relative to the
  // current directory into a fully qualified label. On failure returns an
  // is_null() label and sets the error.
  static Label Resolve(const SourceDir& current_dir,
                       const std::string_view& source_root,
                       ToolchainLabel current_toolchain,
                       const Value& input,
                       Err* err);

  bool is_null() const { return dir_.is_null(); }

  const SourceDir& dir() const { return dir_; }
  const std::string& name() const { return name_.str(); }
  StringAtom name_atom() const { return name_; }

  ToolchainLabel toolchain() const { return toolchain_; }

  // Returns a copy of this label but with an empty toolchain.
  Label GetWithNoToolchain() const;

  // Formats this label in a way that we can present to the user or expose to
  // other parts of the system. SourceDirs end in slashes, but the user
  // expects names like "//chrome/renderer:renderer_config" when printed. The
  // toolchain is optionally included.
  std::string GetUserVisibleName(bool include_toolchain) const;

  // Like the above version, but automatically includes the toolchain if it's
  // not the default one. Normally the user only cares about the toolchain for
  // non-default ones, so this can make certain output more clear.
  std::string GetUserVisibleName(ToolchainLabel default_toolchain) const;

  bool operator==(const Label& other) const {
    return name_.SameAs(other.name_) && dir_ == other.dir_ &&
           toolchain_ == other.toolchain_;
  }
  bool operator!=(const Label& other) const { return !operator==(other); }

  bool operator<(const Label& other) const {
    // Use the fact that these fields are backed by StringAtoms which have
    // very fast equality comparisons to speed this function.
    if (dir_ != other.dir_)
      return dir_ < other.dir_;

    if (!name_.SameAs(other.name_))
      return name_.str() < other.name_.str();

    return toolchain_ < other.toolchain_;
  }

  // Returns true if the toolchain dir/name of this object matches some
  // other object.
  bool ToolchainsEqual(const Label& other) const {
    return toolchain_ == other.toolchain_;
  }

  size_t hash() const { return hash_; }

  // label components (location, name and toolchain) as parsed by
  // ParseLabelString. Note that the toolchain is returned as a single
  // string view here. In case of parsing error, |error| and |error_text| will
  // contain the error message and its help text.
  struct ParseResult {
    std::string_view location;
    std::string_view name;
    std::string_view toolchain;
    const char* error = nullptr;
    const char* error_text = nullptr;
  };

  // Parse a label string into components. A toolchain component is only allowed
  // if |allow_toolchain| is true.
  static ParseResult ParseLabelString(const std::string_view& input,
                                      bool allow_toolchain);

 private:
  Label(SourceDir dir, StringAtom name)
      : dir_(dir), name_(name), hash_(ComputeHash()) {}

  Label(SourceDir dir, StringAtom name, ToolchainLabel toolchain)
      : dir_(dir), name_(name), toolchain_(toolchain), hash_(ComputeHash()) {}

  size_t ComputeHash() const {
    size_t h0 = dir_.hash();
    size_t h1 = name_.hash();
    size_t h2 = toolchain_.hash();
    return (h2 * 131 + h1) * 131 + h0;
  }

  SourceDir dir_;
  StringAtom name_;
  ToolchainLabel toolchain_;
  size_t hash_;
  // NOTE: Must be initialized by constructors with ComputeHash() value.
};

namespace std {

template <>
struct hash<Label> {
  std::size_t operator()(const Label& v) const { return v.hash(); }
};

template <>
struct hash<ToolchainLabel> {
  std::size_t operator()(const ToolchainLabel& v) const { return v.hash(); }
};

}  // namespace std

extern const char kLabels_Help[];

#endif  // TOOLS_GN_LABEL_H_
