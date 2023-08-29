// Copyright (c) 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_GEN_H_
#define TOOLS_GN_GEN_H_

#include "gn/input_alternate_loader.h"

class Gen {
 public:
  static int Run(const std::vector<std::string>& args);
  static int RunWithAlternateLoader(std::function<std::unique_ptr<InputAlternateLoader>(Setup*)> setup_loader, const std::vector<std::string>& args);
};

#endif  // TOOLS_GN_GEN_H_
