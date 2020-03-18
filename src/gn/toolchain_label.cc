// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/toolchain_label.h"

namespace {

// We print user visible label names with no trailing slash after the
// directory name.
std::string_view DirWithNoTrailingSlash(const SourceDir& dir) {
  // Be careful not to trim if the input is just "/" or "//".
  const std::string& value = dir.value();
  if (value.size() > 2)
    return {value.c_str(), value.size() - 1};
  return value;
}

// Verify that |value| is a proper toolchain label, then create a StringAtom
// that can be used as an initializer for a ToolchainLabel value.
StringAtom MakeStringAtom(std::string_view value) {
  // Verify that the format is correct
  if (!value.empty()) {
    // Must start with an absolute path separator.
    DCHECK(value[0] == '/');

    // Must have a colon that is not in terminal position.
    size_t colon = value.find(':');
    DCHECK(colon < value.size() + 1);

    // No directory separator before the colon, unless for /: and //:
    if (value[colon - 1] == '/') {
      DCHECK(colon == 1 || (colon == 2 && value[1] == '/'));
    }

    // No directory separator after the colon.
    DCHECK(value.substr(colon + 1).find('/') == std::string_view::npos);
  }
  return StringAtom(value);
}

// Verify |toolchain_dir| and |name| and create a StringAtom
// that can be used as an initializer for it.
StringAtom MakeStringAtom(const SourceDir& toolchain_dir,
                          const std::string_view& name) {
  std::string ret;
  ret.reserve(toolchain_dir.value().size() + name.size() + 1);

  if (toolchain_dir.value().empty()) {
    // Some unit-test use an empty directory with a name for the toolchain.
    // Otherwise, empty toolchain dir and name should return an empty label.
    if (!name.empty()) {
      ret.append("//:");
      ret.append(name.data(), name.size());
    }
  } else {
    DCHECK(toolchain_dir.value()[0] == '/');
    ret.append(DirWithNoTrailingSlash(toolchain_dir));
    ret.push_back(':');
    ret.append(name.data(), name.size());
  }
  return MakeStringAtom(ret);
}

}  // namespace

ToolchainLabel::ToolchainLabel(std::string_view str)
    : value_(MakeStringAtom(str)) {}

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
