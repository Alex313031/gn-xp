// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_

#include "gn/c_tool.h"
#include "gn/config_values.h"
#include "gn/ninja_target_writer.h"
#include "gn/toolchain.h"
#include "gn/unique_vector.h"

struct EscapeOptions;

// Writes a .ninja file for a binary target type (an executable, a shared
// library, or a static library).
class NinjaBinaryTargetWriter : public NinjaTargetWriter {
 public:
  NinjaBinaryTargetWriter(const Target* target, std::ostream& out);
  ~NinjaBinaryTargetWriter() override;

  void Run() override;

 protected:
  // Writes the stamp line for a source set. These are not linked.
  void WriteSourceSetStamp(const std::vector<OutputFile>& object_files);

  OutputFile WriteStampAndGetDep(const UniqueVector<const SourceFile*>& files,
                                 const std::string& stamp_ext) const;

  void WriteLinkerFlags(std::ostream& out,
                        const Tool* tool,
                        const SourceFile* optional_def_file);
  void WriteFrameworks(std::ostream& out, const Tool* tool);
  void WriteSwiftModules(std::ostream& out,
                         const Tool* tool,
                         const std::vector<OutputFile>& swiftmodules);

 private:
  NinjaBinaryTargetWriter(const NinjaBinaryTargetWriter&) = delete;
  NinjaBinaryTargetWriter& operator=(const NinjaBinaryTargetWriter&) = delete;
};

#endif  // TOOLS_GN_NINJA_BINARY_TARGET_WRITER_H_
