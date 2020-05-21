// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_DESC_BUILDER_H_
#define TOOLS_GN_DESC_BUILDER_H_

#include "base/values.h"
#include "gn/target.h"

class DescBuilder {
 public:
  struct Options {
    bool all = false;
    bool tree = false;
    bool blame = false;
  };

  using DictionaryPtr = std::unique_ptr<base::DictionaryValue>;

  // Creates Dictionary representation for given target
  static DictionaryPtr DescriptionForTarget(const Target* target,
                                            const std::string& what,
                                            const Options& options);

  // Creates Dictionary representation for given config
  static DictionaryPtr DescriptionForConfig(const Config* config,
                                            const std::string& what,
                                            const Options& options);
};

#endif
