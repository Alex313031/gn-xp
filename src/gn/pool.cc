// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/pool.h"

#include <sstream>

#include "base/logging.h"

Pool::~Pool() = default;

Pool* Pool::AsPool() {
  return this;
}

const Pool* Pool::AsPool() const {
  return this;
}

std::string Pool::GetNinjaName(ToolchainLabel default_toolchain) const {
  bool include_toolchain = label().toolchain() != default_toolchain;
  return GetNinjaName(include_toolchain);
}

std::string Pool::GetNinjaName(bool include_toolchain) const {
  std::ostringstream buffer;
  if (include_toolchain) {
    const std::string& toolchain = label().toolchain().str();
    DCHECK(!toolchain.empty() && toolchain[0] == '/');
    for (std::string::size_type i = 2; i < toolchain.size(); ++i) {
      char c = toolchain[i];
      if (c == '/' || c == ':')
        c = '_';
      buffer << c;
    }
    buffer << "_";
  }

  DCHECK(label().dir().is_source_absolute());
  const std::string& label_dir = label().dir().value();
  for (std::string::size_type i = 2; i < label_dir.size(); ++i) {
    buffer << (label_dir[i] == '/' ? '_' : label_dir[i]);
  }
  buffer << label().name();
  return buffer.str();
}
