// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_METADATAWALK_H_
#define TOOLS_GN_METADATAWALK_H_

#include "tools/gn/build_settings.h"
#include "tools/gn/target.h"
#include "tools/gn/unique_vector.h"
#include "tools/gn/value.h"

std::vector<Value> WalkMetadata(
    UniqueVector<const Target*>& targets,
    const std::vector<base::StringPiece>& keys_to_extract,
    const std::vector<base::StringPiece>& keys_to_walk,
    bool rebase_files,
    Err* err);

#endif  // TOOLS_GN_METADATAWALK_H_
