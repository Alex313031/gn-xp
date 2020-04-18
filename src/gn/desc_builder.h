// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_DESC_BUILDER_H_
#define TOOLS_GN_DESC_BUILDER_H_

#include "base/values.h"
#include "gn/target.h"

class DescBuilder {
 public:
  enum Flag {
    NONE = 0,
    ALL = 1u << 0u,
    TREE = 1u << 1u,
    BLAME = 1u << 2u,
    UNRESOLVED = 1u << 3u
  };
  using Flags = unsigned;

  using DictionaryPtr = std::unique_ptr<base::DictionaryValue>;

  // Creates Dictionary representation for given target
  static DictionaryPtr DescriptionForTarget(
      const Target* target,
      const std::string& what,
      Flags flags = NONE);

  // Creates Dictionary representation for given config
  static DictionaryPtr DescriptionForConfig(
      const Config* config,
      const std::string& what,
      Flags flags = NONE);
};

#endif
