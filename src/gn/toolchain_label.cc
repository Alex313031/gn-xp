// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/toolchain_label.h"

#include "gn/label.h"

namespace {

// Create a StringAtom from a (SourceDir,name) tuple.
// This performs a DCHECK() to verify that the result is a valid toolchain
// label.
StringAtom MakeStringAtom(const SourceDir& toolchain_dir,
                          const std::string_view& name) {
  std::string str;
  str.reserve(toolchain_dir.value().size() + name.size() + 1);

  if (toolchain_dir.value().empty()) {
    // Some unit-test use an empty directory with a name for the toolchain.
    // Otherwise, empty toolchain dir and name should return an empty label.
    if (!name.empty()) {
      str.append("//:");
      str.append(name.data(), name.size());
    }
  } else {
    str.append(toolchain_dir.SourceWithNoTrailingSlash());
    str.push_back(':');
    str.append(name.data(), name.size());
  }

  // Sanity check that this is a correct label.
  if (!str.empty())
    DCHECK(!Label::ParseLabelString(str, false).error);

  return StringAtom(str);
}

}  // namespace

ToolchainLabel::ToolchainLabel(const SourceDir& dir,
                               const std::string_view name)
    : value_(MakeStringAtom(dir, name)) {}

SourceDir ToolchainLabel::dir() const {
  std::string_view value = value_.str();
  size_t pos = value.find(':');
  if (pos == std::string_view::npos)
    return SourceDir();  // Assume empty.
  else
    return SourceDir(value.substr(0, pos));
}

std::string_view ToolchainLabel::name() const {
  std::string_view value = value_.str();
  size_t pos = value.find(':');
  if (pos == std::string_view::npos)
    return {};  // Assume empty.
  else
    return value.substr(pos + 1);
}

std::string ToolchainLabel::GetOutputDir() const {
  // For now just assume the toolchain name is always a valid dir name. We may
  // want to clean up the in the future.
  std::string result;

  std::string_view value = value_.str();
  size_t pos = value.find(':');
  if (pos != std::string_view::npos) {
    result = value.substr(pos + 1);
    result += '/';
  }
  return result;
}
