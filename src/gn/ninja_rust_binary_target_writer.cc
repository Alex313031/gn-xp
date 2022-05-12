// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_rust_binary_target_writer.h"

#include <sstream>

#include "base/strings/string_util.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_target_command_util.h"
#include "gn/ninja_utils.h"
#include "gn/rust_substitution_type.h"
#include "gn/rust_values.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;
  return opts;
}

void WriteVar(const char* name,
              const std::string& value,
              EscapeOptions opts,
              std::ostream& out) {
  out << name << " = ";
  EscapeStringToStream(out, value, opts);
  out << std::endl;
}

void WriteCrateVars(const Target* target,
                    const Tool* tool,
                    EscapeOptions opts,
                    std::ostream& out) {
  WriteVar(kRustSubstitutionCrateName.ninja_name,
           target->rust_values().crate_name(), opts, out);

  std::string crate_type;
  switch (target->rust_values().crate_type()) {
    // Auto-select the crate type for executables, static libraries, and rlibs.
    case RustValues::CRATE_AUTO: {
      switch (target->output_type()) {
        case Target::EXECUTABLE:
          crate_type = "bin";
          break;
        case Target::STATIC_LIBRARY:
          crate_type = "staticlib";
          break;
        case Target::RUST_LIBRARY:
          crate_type = "rlib";
          break;
        case Target::RUST_PROC_MACRO:
          crate_type = "proc-macro";
          break;
        default:
          NOTREACHED();
      }
      break;
    }
    case RustValues::CRATE_BIN:
      crate_type = "bin";
      break;
    case RustValues::CRATE_CDYLIB:
      crate_type = "cdylib";
      break;
    case RustValues::CRATE_DYLIB:
      crate_type = "dylib";
      break;
    case RustValues::CRATE_PROC_MACRO:
      crate_type = "proc-macro";
      break;
    case RustValues::CRATE_RLIB:
      crate_type = "rlib";
      break;
    case RustValues::CRATE_STATICLIB:
      crate_type = "staticlib";
      break;
    default:
      NOTREACHED();
  }
  WriteVar(kRustSubstitutionCrateType.ninja_name, crate_type, opts, out);

  WriteVar(SubstitutionOutputExtension.ninja_name,
           SubstitutionWriter::GetLinkerSubstitution(
               target, tool, &SubstitutionOutputExtension),
           opts, out);
  WriteVar(SubstitutionOutputDir.ninja_name,
           SubstitutionWriter::GetLinkerSubstitution(target, tool,
                                                     &SubstitutionOutputDir),
           opts, out);
}

}  // namespace

NinjaRustBinaryTargetWriter::NinjaRustBinaryTargetWriter(const Target* target,
                                                         std::ostream& out)
    : NinjaBinaryTargetWriter(target, out),
      tool_(target->toolchain()->GetToolForTargetFinalOutputAsRust(target)) {}

NinjaRustBinaryTargetWriter::~NinjaRustBinaryTargetWriter() = default;

// TODO(juliehockett): add inherited library support? and IsLinkable support?
// for c-cross-compat
void NinjaRustBinaryTargetWriter::Run() {
  DCHECK(target_->output_type() != Target::SOURCE_SET);

  size_t num_stamp_uses = target_->sources().size();

  std::vector<OutputFile> input_deps =
      WriteInputsStampAndGetDep(num_stamp_uses);

  WriteCompilerVars();
  WriteRustExternsAndDeps();
  WriteSourcesAndInputs();
}

void NinjaRustBinaryTargetWriter::WriteCompilerVars() {
  const SubstitutionBits& subst = target_->toolchain()->substitution_bits();

  EscapeOptions opts = GetFlagOptions();
  WriteCrateVars(target_, tool_, opts, out_);

  WriteRustCompilerVars(subst, /*indent=*/false, /*always_write=*/true);

  WriteSharedVars(subst);
}

void NinjaRustBinaryTargetWriter::AppendSourcesAndInputsToImplicitDeps(
    UniqueVector<OutputFile>* deps) const {
  // Only the crate_root file needs to be given to rustc as input.
  // Any other 'sources' are just implicit deps.
  // Most Rust targets won't bother specifying the "sources =" line
  // because it is handled sufficiently by crate_root and the generation
  // of depfiles by rustc. But for those which do...
  for (const auto& source : target_->sources()) {
    deps->push_back(OutputFile(settings_->build_settings(), source));
  }
  for (const auto& data : target_->config_values().inputs()) {
    deps->push_back(OutputFile(settings_->build_settings(), data));
  }
}

void NinjaRustBinaryTargetWriter::WriteSourcesAndInputs() {
  out_ << "  sources =";
  for (const auto& source : target_->sources()) {
    out_ << " ";
    path_output_.WriteFile(out_,
                           OutputFile(settings_->build_settings(), source));
  }
  for (const auto& data : target_->config_values().inputs()) {
    out_ << " ";
    path_output_.WriteFile(out_, OutputFile(settings_->build_settings(), data));
  }
  out_ << std::endl;
}
