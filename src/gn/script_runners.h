// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_SCRIPT_RUNNERS_H_
#define TOOLS_GN_SCRIPT_RUNNERS_H_

#include <map>

#include "base/files/file_path.h"
#include "gn/scope.h"

class Err;
class SourceFile;
class Value;

// Manages the registry of script runners. It stores the mapping a a script
// runner name to the path to the script interpreter binary.
class ScriptRunners {
 public:
  using ResolveRunnerPathCallback =
      std::function<base::FilePath(const std::string&)>;

  ScriptRunners();
  ScriptRunners(const ScriptRunners& other);
  ~ScriptRunners();

  // Defines the set of script runners. Any existing definitions will be
  // cleared.
  bool DefineScriptRunnersFromScope(const Scope::KeyValueMap& runners,
                                    const Scope& scope,
                                    Err* err);

  // Adds a single script runner. This should normally only used to set up
  // implicit defaults as part of initialization.
  void AddScriptRunner(const std::string_view name,
                       const base::FilePath& value);

  // Returns the binary path for for the given runner name.
  base::FilePath GetPathForRunner(const Value& name, Err* err) const;

  // Returns true if the script runners were explicitly set in the build config.
  bool explicitly_defined() const { return explicitly_defined_; }
  void set_explicitly_defined(bool v) { explicitly_defined_ = v; }

  // Called to resolve the path to use for a given runner when the script
  // runners are set. Only really useful to override the default behavior
  // in tests.
  void set_resolve_runner_path_callback(ResolveRunnerPathCallback cb) {
    resolve_runner_path_callback_ = std::move(cb);
  }

 private:
  base::FilePath ResolveRunnerPath(const Value& value,
                                   const Scope& scope,
                                   Err* err);

  // No lock is used as these values are only expected to be written in two
  // places: Once during init to register the implicit "python" runner (for
  // backwards compatiblity), and once afterward when when processing the
  // BUILDCONFIG to set any explicit definitions there.
  // It is otherwise only read from.

  // Mapping of runner names to binary paths
  std::map<std::string_view, base::FilePath> path_map_;

  ResolveRunnerPathCallback resolve_runner_path_callback_;

  // True if these values were set through BUILDCONFIG.
  bool explicitly_defined_;

  DISALLOW_ASSIGN(ScriptRunners);
};

#endif  // TOOLS_GN_SCRIPT_RUNNERS_H_
