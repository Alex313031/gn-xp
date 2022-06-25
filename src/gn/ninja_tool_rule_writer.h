// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_TOOL_RULE_WRITER_H_
#define TOOLS_GN_NINJA_TOOL_RULE_WRITER_H_

#include <iosfwd>
#include <string>

#include "gn/escape.h"
#include "gn/substitution_pattern.h"

class Settings;
class Tool;

class NinjaToolRuleWriter {
 public:
  // Takes the settings for the toolchain and an individual tool and writes a
  // single rule to the given ostream. Rule name based on toolchain and tool
  // name.
  static void WriteToolRule(const Settings* settings,
                            const Tool* tool,
                            std::ostream& out);

  // Takes the settings for the toolchain and an individual tool and writes a
  // single rule to the given ostream using the given name.
  static void WriteToolRuleWithName(const std::string& rule_name,
                                    const Settings* settings,
                                    const Tool* tool,
                                    std::ostream& out);

 private:
  NinjaToolRuleWriter(const Settings* settings,
                      const Tool* tool,
                      std::ostream& out);
  ~NinjaToolRuleWriter();

  void WriteRule(const std::string& rule_name);
  void WriteRulePattern(const char* name,
                        const SubstitutionPattern& pattern,
                        const EscapeOptions& options);
  void WriteCommandRulePattern(const char* name,
                               const std::string& launcher,
                               const SubstitutionPattern& command,
                               const EscapeOptions& options);

  const Settings* settings_;
  const Tool* tool_;
  std::ostream& out_;

  NinjaToolRuleWriter(const NinjaToolRuleWriter&) = delete;
  NinjaToolRuleWriter& operator=(const NinjaToolRuleWriter&) = delete;
};

#endif  // TOOLS_GN_NINJA_TOOL_RULE_WRITER_H_
