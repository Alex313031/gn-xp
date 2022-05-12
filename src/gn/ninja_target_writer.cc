// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/ninja_target_writer.h"

#include <sstream>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "gn/c_substitution_type.h"
#include "gn/config_values_extractors.h"
#include "gn/err.h"
#include "gn/escape.h"
#include "gn/filesystem_utils.h"
#include "gn/general_tool.h"
#include "gn/ninja_action_target_writer.h"
#include "gn/ninja_binary_target_writer.h"
#include "gn/ninja_bundle_data_target_writer.h"
#include "gn/ninja_copy_target_writer.h"
#include "gn/ninja_create_bundle_target_writer.h"
#include "gn/ninja_generated_file_target_writer.h"
#include "gn/ninja_group_target_writer.h"
#include "gn/ninja_target_command_util.h"
#include "gn/ninja_utils.h"
#include "gn/output_file.h"
#include "gn/rust_substitution_type.h"
#include "gn/scheduler.h"
#include "gn/string_output_buffer.h"
#include "gn/string_utils.h"
#include "gn/substitution_writer.h"
#include "gn/target.h"
#include "gn/trace.h"
#include "rust_tool.h"

NinjaTargetWriter::NinjaTargetWriter(const Target* target, std::ostream& out)
    : settings_(target->settings()),
      target_(target),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   settings_->build_settings()->root_path_utf8(),
                   ESCAPE_NINJA) {}

NinjaTargetWriter::~NinjaTargetWriter() = default;

// static
std::string NinjaTargetWriter::RunAndWriteFile(const Target* target) {
  const Settings* settings = target->settings();

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE,
                    target->label().GetUserVisibleName(false));
  trace.SetToolchain(settings->toolchain_label());

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Computing", target->label().GetUserVisibleName(true));

  // It's ridiculously faster to write to a string and then write that to
  // disk in one operation than to use an fstream here.
  StringOutputBuffer storage;
  std::ostream rules(&storage);

  // Call out to the correct sub-type of writer. Binary targets need to be
  // written to separate files for compiler flag scoping, but other target
  // types can have their rules coalesced.
  //
  // In ninja, if a rule uses a variable (like $include_dirs) it will use
  // the value set by indenting it under the build line or it takes the value
  // from the end of the invoking scope (otherwise the current file). It does
  // not copy the value from what it was when the build line was encountered.
  // To avoid writing lots of duplicate rules for defines and cflags, etc. on
  // each source file build line, we use separate .ninja files with the shared
  // variables set at the top.
  //
  // Groups and actions don't use this type of flag, they make unique rules
  // or write variables scoped under each build line. As a result, they don't
  // need the separate files.
  bool needs_file_write = false;
  if (target->output_type() == Target::BUNDLE_DATA) {
    NinjaBundleDataTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::CREATE_BUNDLE) {
    NinjaCreateBundleTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::COPY_FILES) {
    NinjaCopyTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::ACTION ||
             target->output_type() == Target::ACTION_FOREACH) {
    NinjaActionTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::GROUP) {
    NinjaGroupTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->output_type() == Target::GENERATED_FILE) {
    NinjaGeneratedFileTargetWriter writer(target, rules);
    writer.Run();
  } else if (target->IsBinary()) {
    needs_file_write = true;
    NinjaBinaryTargetWriter writer(target, rules);
    writer.Run();
  } else {
    CHECK(0) << "Output type of target not handled.";
  }

  if (needs_file_write) {
    // Write the ninja file.
    SourceFile ninja_file = GetNinjaFileForTarget(target);
    base::FilePath full_ninja_file =
        settings->build_settings()->GetFullPath(ninja_file);
    storage.WriteToFileIfChanged(full_ninja_file, nullptr);

    EscapeOptions options;
    options.mode = ESCAPE_NINJA;

    // Return the subninja command to load the rules file.
    std::string result = "subninja ";
    result.append(EscapeString(
        OutputFile(target->settings()->build_settings(), ninja_file).value(),
        options, nullptr));
    result.push_back('\n');
    return result;
  }

  // No separate file required, just return the rules.
  return storage.str();
}

void NinjaTargetWriter::WriteEscapedSubstitution(const Substitution* type) {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA;

  out_ << type->ninja_name << " = ";
  EscapeStringToStream(
      out_, SubstitutionWriter::GetTargetSubstitution(target_, type), opts);
  out_ << std::endl;
}

void NinjaTargetWriter::WriteSharedVars(const SubstitutionBits& bits) {
  bool written_anything = false;

  // Target label.
  if (bits.used.count(&SubstitutionLabel)) {
    WriteEscapedSubstitution(&SubstitutionLabel);
    written_anything = true;
  }

  // Target label name.
  if (bits.used.count(&SubstitutionLabelName)) {
    WriteEscapedSubstitution(&SubstitutionLabelName);
    written_anything = true;
  }

  // Target label name without toolchain.
  if (bits.used.count(&SubstitutionLabelNoToolchain)) {
    WriteEscapedSubstitution(&SubstitutionLabelNoToolchain);
    written_anything = true;
  }

  // Root gen dir.
  if (bits.used.count(&SubstitutionRootGenDir)) {
    WriteEscapedSubstitution(&SubstitutionRootGenDir);
    written_anything = true;
  }

  // Root out dir.
  if (bits.used.count(&SubstitutionRootOutDir)) {
    WriteEscapedSubstitution(&SubstitutionRootOutDir);
    written_anything = true;
  }

  // Target gen dir.
  if (bits.used.count(&SubstitutionTargetGenDir)) {
    WriteEscapedSubstitution(&SubstitutionTargetGenDir);
    written_anything = true;
  }

  // Target out dir.
  if (bits.used.count(&SubstitutionTargetOutDir)) {
    WriteEscapedSubstitution(&SubstitutionTargetOutDir);
    written_anything = true;
  }

  // Target output name.
  if (bits.used.count(&SubstitutionTargetOutputName)) {
    WriteEscapedSubstitution(&SubstitutionTargetOutputName);
    written_anything = true;
  }

  // If we wrote any vars, separate them from the rest of the file that follows
  // with a blank line.
  if (written_anything)
    out_ << std::endl;
}

void NinjaTargetWriter::WriteCCompilerVars(const SubstitutionBits& bits,
                                           bool indent,
                                           bool respect_source_used) {
  // Defines.
  if (bits.used.count(&CSubstitutionDefines)) {
    if (indent)
      out_ << "  ";
    out_ << CSubstitutionDefines.ninja_name << " =";
    RecursiveTargetConfigToStream<std::string>(kRecursiveWriterSkipDuplicates,
                                               target_, &ConfigValues::defines,
                                               DefineWriter(), out_);
    out_ << std::endl;
  }

  // Framework search path.
  if (bits.used.count(&CSubstitutionFrameworkDirs)) {
    const Tool* tool = target_->toolchain()->GetTool(CTool::kCToolLink);

    if (indent)
      out_ << "  ";
    out_ << CSubstitutionFrameworkDirs.ninja_name << " =";
    PathOutput framework_dirs_output(
        path_output_.current_dir(),
        settings_->build_settings()->root_path_utf8(), ESCAPE_NINJA_COMMAND);
    RecursiveTargetConfigToStream<SourceDir>(
        kRecursiveWriterSkipDuplicates, target_, &ConfigValues::framework_dirs,
        FrameworkDirsWriter(framework_dirs_output,
                            tool->framework_dir_switch()),
        out_);
    out_ << std::endl;
  }

  // Include directories.
  if (bits.used.count(&CSubstitutionIncludeDirs)) {
    if (indent)
      out_ << "  ";
    out_ << CSubstitutionIncludeDirs.ninja_name << " =";
    PathOutput include_path_output(
        path_output_.current_dir(),
        settings_->build_settings()->root_path_utf8(), ESCAPE_NINJA_COMMAND);
    RecursiveTargetConfigToStream<SourceDir>(
        kRecursiveWriterSkipDuplicates, target_, &ConfigValues::include_dirs,
        IncludeWriter(include_path_output), out_);
    out_ << std::endl;
  }

  bool has_precompiled_headers =
      target_->config_values().has_precompiled_headers();

  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;
  if (respect_source_used
          ? target_->source_types_used().Get(SourceFile::SOURCE_S)
          : bits.used.count(&CSubstitutionAsmFlags)) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &CSubstitutionAsmFlags, false, Tool::kToolNone,
                 &ConfigValues::asmflags, opts, path_output_, out_, true,
                 indent);
  }
  if (respect_source_used
          ? (target_->source_types_used().Get(SourceFile::SOURCE_C) ||
             target_->source_types_used().Get(SourceFile::SOURCE_CPP) ||
             target_->source_types_used().Get(SourceFile::SOURCE_M) ||
             target_->source_types_used().Get(SourceFile::SOURCE_MM) ||
             target_->source_types_used().Get(SourceFile::SOURCE_MODULEMAP))
          : bits.used.count(&CSubstitutionCFlags)) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_, &CSubstitutionCFlags,
                 false, Tool::kToolNone, &ConfigValues::cflags, opts,
                 path_output_, out_, true, indent);
  }
  if (respect_source_used
          ? target_->source_types_used().Get(SourceFile::SOURCE_C)
          : bits.used.count(&CSubstitutionCFlagsC)) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_, &CSubstitutionCFlagsC,
                 has_precompiled_headers, CTool::kCToolCc,
                 &ConfigValues::cflags_c, opts, path_output_, out_, true,
                 indent);
  }
  if (respect_source_used
          ? (target_->source_types_used().Get(SourceFile::SOURCE_CPP) ||
             target_->source_types_used().Get(SourceFile::SOURCE_MODULEMAP))
          : bits.used.count(&CSubstitutionCFlagsCc)) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &CSubstitutionCFlagsCc, has_precompiled_headers,
                 CTool::kCToolCxx, &ConfigValues::cflags_cc, opts, path_output_,
                 out_, true, indent);
  }
  if (respect_source_used
          ? target_->source_types_used().Get(SourceFile::SOURCE_M)
          : bits.used.count(&CSubstitutionCFlagsObjC)) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &CSubstitutionCFlagsObjC, has_precompiled_headers,
                 CTool::kCToolObjC, &ConfigValues::cflags_objc, opts,
                 path_output_, out_, true, indent);
  }
  if (respect_source_used
          ? target_->source_types_used().Get(SourceFile::SOURCE_MM)
          : bits.used.count(&CSubstitutionCFlagsObjCc)) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &CSubstitutionCFlagsObjCc, has_precompiled_headers,
                 CTool::kCToolObjCxx, &ConfigValues::cflags_objcc, opts,
                 path_output_, out_, true, indent);
  }
  if (target_->source_types_used().SwiftSourceUsed() || !respect_source_used) {
    if (bits.used.count(&CSubstitutionSwiftModuleName)) {
      if (indent)
        out_ << "  ";
      out_ << CSubstitutionSwiftModuleName.ninja_name << " = ";
      EscapeStringToStream(out_, target_->swift_values().module_name(), opts);
      out_ << std::endl;
    }

    if (bits.used.count(&CSubstitutionSwiftBridgeHeader)) {
      if (indent)
        out_ << "  ";
      out_ << CSubstitutionSwiftBridgeHeader.ninja_name << " = ";
      if (!target_->swift_values().bridge_header().is_null()) {
        path_output_.WriteFile(out_, target_->swift_values().bridge_header());
      } else {
        out_ << R"("")";
      }
      out_ << std::endl;
    }

    if (bits.used.count(&CSubstitutionSwiftModuleDirs)) {
      // Uniquify the list of swiftmodule dirs (in case multiple swiftmodules
      // are generated in the same directory).
      UniqueVector<SourceDir> swiftmodule_dirs;
      for (const Target* dep : target_->swift_values().modules())
        swiftmodule_dirs.push_back(dep->swift_values().module_output_dir());

      if (indent)
        out_ << "  ";
      out_ << CSubstitutionSwiftModuleDirs.ninja_name << " =";
      PathOutput swiftmodule_path_output(
          path_output_.current_dir(),
          settings_->build_settings()->root_path_utf8(), ESCAPE_NINJA_COMMAND);
      IncludeWriter swiftmodule_path_writer(swiftmodule_path_output);
      for (const SourceDir& swiftmodule_dir : swiftmodule_dirs) {
        swiftmodule_path_writer(swiftmodule_dir, out_);
      }
      out_ << std::endl;
    }

    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &CSubstitutionSwiftFlags, false, CTool::kCToolSwift,
                 &ConfigValues::swiftflags, opts, path_output_, out_, true,
                 indent);
  }
}

void NinjaTargetWriter::WriteRustCompilerVars(const SubstitutionBits& bits,
                                              bool indent,
                                              bool always_write) {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;

  if (bits.used.count(&kRustSubstitutionRustFlags) || always_write) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &kRustSubstitutionRustFlags, false, Tool::kToolNone,
                 &ConfigValues::rustflags, opts, path_output_, out_, true,
                 indent);
  }

  if (bits.used.count(&kRustSubstitutionRustEnv) || always_write) {
    WriteOneFlag(kRecursiveWriterKeepDuplicates, target_,
                 &kRustSubstitutionRustEnv, false, Tool::kToolNone,
                 &ConfigValues::rustenv, opts, path_output_, out_, true,
                 indent);
  }

  if (bits.used.count(&kRustSubstitutionRustDeps) || always_write) {
    WriteRustExternsAndDeps();
  }
}

void NinjaTargetWriter::WriteRustExternsAndDeps() {
  const RustTool* tool_ =
      target_->toolchain()->GetToolForTargetFinalOutputAsRust(target_);

  // Classify our dependencies.
  ClassifiedDeps classified_deps = GetClassifiedDeps();

  // The input dependencies will be an order-only dependency. This will cause
  // Ninja to make sure the inputs are up to date before compiling this
  // source, but changes in the inputs deps won't cause the file to be
  // recompiled. See the comment on NinjaCBinaryTargetWriter::Run for more
  // detailed explanation.
  std::vector<OutputFile> order_only_deps = WriteInputDepsStampAndGetDep(
      std::vector<const Target*>(), num_stamp_uses);
  std::copy(input_deps.begin(), input_deps.end(),
            std::back_inserter(order_only_deps));

  // Build lists which will go into different bits of the rustc command line.
  // Public rust_library deps go in a --extern rlibs, public non-rust deps go
  // in -Ldependency. Also assemble a list of extra (i.e. implicit) deps for
  // ninja dependency tracking.
  UniqueVector<OutputFile> implicit_deps;
  AppendSourcesAndInputsToImplicitDeps(&implicit_deps);
  implicit_deps.Append(classified_deps.extra_object_files.begin(),
                       classified_deps.extra_object_files.end());

  std::vector<OutputFile> rustdeps;
  std::vector<OutputFile> nonrustdeps;
  nonrustdeps.insert(nonrustdeps.end(),
                     classified_deps.extra_object_files.begin(),
                     classified_deps.extra_object_files.end());
  for (const auto* framework_dep : classified_deps.framework_deps) {
    order_only_deps.push_back(framework_dep->dependency_output_file());
  }
  for (const auto* non_linkable_dep : classified_deps.non_linkable_deps) {
    if (non_linkable_dep->source_types_used().RustSourceUsed() &&
        non_linkable_dep->output_type() != Target::SOURCE_SET) {
      rustdeps.push_back(non_linkable_dep->dependency_output_file());
    }
    order_only_deps.push_back(non_linkable_dep->dependency_output_file());
  }
  for (const auto* linkable_dep : classified_deps.linkable_deps) {
    // Rust cdylibs are treated as non-Rust dependencies for linking purposes.
    if (linkable_dep->source_types_used().RustSourceUsed() &&
        linkable_dep->rust_values().crate_type() != RustValues::CRATE_CDYLIB) {
      rustdeps.push_back(linkable_dep->link_output_file());
    } else {
      nonrustdeps.push_back(linkable_dep->link_output_file());
    }
    implicit_deps.push_back(linkable_dep->dependency_output_file());
  }

  // Rust libraries specified by paths.
  for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    for (const auto& e : cur.externs()) {
      if (e.second.is_source_file()) {
        implicit_deps.push_back(
            OutputFile(settings_->build_settings(), e.second.source_file()));
      }
    }
  }

  // Collect the full transitive set of rust libraries that this target
  // depends on, and the public flag represents if the target has direct
  // access to the dependency through a chain of public_deps.
  std::vector<ExternCrate> transitive_crates;
  for (const auto& [dep, has_direct_access] :
       target_->rust_transitive_inherited_libs().GetOrderedAndPublicFlag()) {
    // We will tell rustc to look for crate metadata for any rust crate
    // dependencies except cdylibs, as they have no metadata present.
    if (dep->source_types_used().RustSourceUsed() &&
        RustValues::IsRustLibrary(dep)) {
      transitive_crates.push_back({dep, has_direct_access});
      // If the current crate can directly acccess the `dep` crate, then the
      // current crate needs an implicit dependency on `dep` so it will be
      // rebuilt if `dep` changes.
      if (has_direct_access) {
        implicit_deps.push_back(dep->dependency_output_file());
      }
    }
  }

  std::vector<OutputFile> tool_outputs;
  SubstitutionWriter::ApplyListToLinkerAsOutputFile(
      target_, tool_, tool_->outputs(), &tool_outputs);
  WriteCompilerBuildLine({target_->rust_values().crate_root()},
                         implicit_deps.vector(), order_only_deps, tool_->name(),
                         tool_outputs);

  std::vector<const Target*> extern_deps(
      classified_deps.linkable_deps.vector());
  std::copy(classified_deps.non_linkable_deps.begin(),
            classified_deps.non_linkable_deps.end(),
            std::back_inserter(extern_deps));
  WriteExternsAndDeps(tool_, extern_deps, transitive_crates, rustdeps,
                      nonrustdeps);
}

NinjaTargetWriter::ClassifiedDeps NinjaTargetWriter::GetClassifiedDeps() const {
  ClassifiedDeps classified_deps;

  // Normal public/private deps.
  for (const auto& pair : target_->GetDeps(Target::DEPS_LINKED)) {
    ClassifyDependency(pair.ptr, &classified_deps);
  }

  // Inherited libraries.
  for (auto* inherited_target : target_->inherited_libraries().GetOrdered()) {
    ClassifyDependency(inherited_target, &classified_deps);
  }

  // Data deps.
  for (const auto& data_dep_pair : target_->data_deps())
    classified_deps.non_linkable_deps.push_back(data_dep_pair.ptr);

  return classified_deps;
}

std::vector<OutputFile> NinjaTargetWriter::WriteInputDepsStampAndGetDep(
    const std::vector<const Target*>& additional_hard_deps,
    size_t num_stamp_uses) const {
  CHECK(target_->toolchain()) << "Toolchain not set on target "
                              << target_->label().GetUserVisibleName(true);

  // ----------
  // Collect all input files that are input deps of this target. Knowing the
  // number before writing allows us to either skip writing the input deps
  // stamp or optimize it. Use pointers to avoid copies here.
  std::vector<const SourceFile*> input_deps_sources;
  input_deps_sources.reserve(32);

  // Actions get implicit dependencies on the script itself.
  if (target_->output_type() == Target::ACTION ||
      target_->output_type() == Target::ACTION_FOREACH)
    input_deps_sources.push_back(&target_->action_values().script());

  // Input files are only considered for non-binary targets which use an
  // implicit dependency instead. The implicit dependency in this case is
  // handled separately by the binary target writer.
  if (!target_->IsBinary()) {
    for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
      for (const auto& input : iter.cur().inputs())
        input_deps_sources.push_back(&input);
    }
  }

  // For an action (where we run a script only once) the sources are the same
  // as the inputs. For action_foreach, the sources will be operated on
  // separately so don't handle them here.
  if (target_->output_type() == Target::ACTION) {
    for (const auto& source : target_->sources())
      input_deps_sources.push_back(&source);
  }

  // ----------
  // Collect all target input dependencies of this target as was done for the
  // files above.
  std::vector<const Target*> input_deps_targets;
  input_deps_targets.reserve(32);

  // Hard dependencies that are direct or indirect dependencies.
  // These are large (up to 100s), hence why we check other
  const TargetSet& hard_deps(target_->recursive_hard_deps());
  for (const Target* target : hard_deps) {
    // BUNDLE_DATA should normally be treated as a data-only dependency
    // (see Target::IsDataOnly()). Only the CREATE_BUNDLE target, that actually
    // consumes this data, needs to have the BUNDLE_DATA as an input dependency.
    if (target->output_type() != Target::BUNDLE_DATA ||
        target_->output_type() == Target::CREATE_BUNDLE)
      input_deps_targets.push_back(target);
  }

  // Additional hard dependencies passed in. These are usually empty or small,
  // and we don't want to duplicate the explicit hard deps of the target.
  for (const Target* target : additional_hard_deps) {
    if (!hard_deps.contains(target))
      input_deps_targets.push_back(target);
  }

  // Toolchain dependencies. These must be resolved before doing anything.
  // This just writes all toolchain deps for simplicity. If we find that
  // toolchains often have more than one dependency, we could consider writing
  // a toolchain-specific stamp file and only include the stamp here.
  // Note that these are usually empty/small.
  const LabelTargetVector& toolchain_deps = target_->toolchain()->deps();
  for (const auto& toolchain_dep : toolchain_deps) {
    // This could theoretically duplicate dependencies already in the list,
    // but it shouldn't happen in practice, is inconvenient to check for,
    // and only results in harmless redundant dependencies listed.
    input_deps_targets.push_back(toolchain_dep.ptr);
  }

  // ---------
  // Write the outputs.

  if (input_deps_sources.size() + input_deps_targets.size() == 0)
    return std::vector<OutputFile>();  // No input dependencies.

  // If we're only generating one input dependency, return it directly instead
  // of writing a stamp file for it.
  if (input_deps_sources.size() == 1 && input_deps_targets.size() == 0)
    return std::vector<OutputFile>{
        OutputFile(settings_->build_settings(), *input_deps_sources[0])};
  if (input_deps_sources.size() == 0 && input_deps_targets.size() == 1) {
    const OutputFile& dep = input_deps_targets[0]->dependency_output_file();
    DCHECK(!dep.value().empty());
    return std::vector<OutputFile>{dep};
  }

  std::vector<OutputFile> outs;
  // File input deps.
  for (const SourceFile* source : input_deps_sources)
    outs.push_back(OutputFile(settings_->build_settings(), *source));
  // Target input deps. Sort by label so the output is deterministic (otherwise
  // some of the targets will have gone through std::sets which will have
  // sorted them by pointer).
  std::sort(
      input_deps_targets.begin(), input_deps_targets.end(),
      [](const Target* a, const Target* b) { return a->label() < b->label(); });
  for (auto* dep : input_deps_targets) {
    DCHECK(!dep->dependency_output_file().value().empty());
    outs.push_back(dep->dependency_output_file());
  }

  // If there are multiple inputs, but the stamp file would be referenced only
  // once, don't write it but depend on the inputs directly.
  if (num_stamp_uses == 1u)
    return outs;

  // Make a stamp file.
  OutputFile input_stamp_file =
      GetBuildDirForTargetAsOutputFile(target_, BuildDirType::OBJ);
  input_stamp_file.value().append(target_->label().name());
  input_stamp_file.value().append(".inputdeps.stamp");

  out_ << "build ";
  path_output_.WriteFile(out_, input_stamp_file);
  out_ << ": " << GetNinjaRulePrefixForToolchain(settings_)
       << GeneralTool::kGeneralToolStamp;
  path_output_.WriteFiles(out_, outs);

  out_ << "\n";
  return std::vector<OutputFile>{input_stamp_file};
}

void NinjaTargetWriter::WriteStampForTarget(
    const std::vector<OutputFile>& files,
    const std::vector<OutputFile>& order_only_deps) {
  const OutputFile& stamp_file = target_->dependency_output_file();

  // First validate that the target's dependency is a stamp file. Otherwise,
  // we shouldn't have gotten here!
  CHECK(base::EndsWith(stamp_file.value(), ".stamp",
                       base::CompareCase::INSENSITIVE_ASCII))
      << "Output should end in \".stamp\" for stamp file output. Instead got: "
      << "\"" << stamp_file.value() << "\"";

  out_ << "build ";
  path_output_.WriteFile(out_, stamp_file);

  out_ << ": " << GetNinjaRulePrefixForToolchain(settings_)
       << GeneralTool::kGeneralToolStamp;
  path_output_.WriteFiles(out_, files);

  if (!order_only_deps.empty()) {
    out_ << " ||";
    path_output_.WriteFiles(out_, order_only_deps);
  }
  out_ << std::endl;
}

void NinjaTargetWriter::WriteExternsAndDeps(
    const RustTool* tool_,
    const std::vector<const Target*>& deps,
    const std::vector<ExternCrate>& transitive_rust_deps,
    const std::vector<OutputFile>& rustdeps,
    const std::vector<OutputFile>& nonrustdeps) {
  // Writes a external LibFile which comes from user-specified externs, and may
  // be either a string or a SourceFile.
  auto write_extern_lib_file = [this](std::string_view crate_name,
                                      LibFile lib_file) {
    out_ << " --extern ";
    out_ << crate_name;
    out_ << "=";
    if (lib_file.is_source_file()) {
      path_output_.WriteFile(out_, lib_file.source_file());
    } else {
      EscapeOptions escape_opts_command;
      escape_opts_command.mode = ESCAPE_NINJA_COMMAND;
      EscapeStringToStream(out_, lib_file.value(), escape_opts_command);
    }
  };
  // Writes an external OutputFile which comes from a dependency of the current
  // target.
  auto write_extern_target = [this](const Target& dep) {
    std::string_view crate_name;
    const auto& aliased_deps = target_->rust_values().aliased_deps();
    if (auto it = aliased_deps.find(dep.label()); it != aliased_deps.end()) {
      crate_name = it->second;
    } else {
      crate_name = dep.rust_values().crate_name();
    }

    out_ << " --extern ";
    out_ << crate_name;
    out_ << "=";
    path_output_.WriteFile(out_, dep.dependency_output_file());
  };

  // Write accessible crates with `--extern` to add them to the extern prelude.
  out_ << "  externs =";

  // Tracking to avoid emitted the same lib twice. We track it instead of
  // pre-emptively constructing a UniqueVector since we would have to also store
  // the crate name, and in the future the public-ness.
  std::unordered_set<OutputFile> emitted_rust_libs;
  // TODO: We defer private dependencies to -Ldependency until --extern priv is
  // stabilized.
  UniqueVector<SourceDir> private_extern_dirs;

  // Walk the transitive closure of all rust dependencies.
  //
  // For dependencies that are meant to be accessible we pass them to --extern
  // in order to add them to the crate's extern prelude.
  //
  // For all transitive dependencies, we add them to `private_extern_dirs` in
  // order to generate a -Ldependency switch that points to them. This ensures
  // that rustc can find them if they are used by other dependencies. For
  // example:
  //
  //   A -> C --public--> D
  //     -> B --private-> D
  //
  // Here A has direct access to D, but B and C also make use of D, and they
  // will only search the paths specified to -Ldependency, thus D needs to
  // appear as both a --extern (for A) and -Ldependency (for B and C).
  for (const auto& crate : transitive_rust_deps) {
    const OutputFile& rust_lib = crate.target->dependency_output_file();
    if (emitted_rust_libs.count(rust_lib) == 0) {
      if (crate.has_direct_access) {
        write_extern_target(*crate.target);
      }
      emitted_rust_libs.insert(rust_lib);
    }
    private_extern_dirs.push_back(
        rust_lib.AsSourceFile(settings_->build_settings()).GetDir());
  }

  // Add explicitly specified externs from the GN target.
  for (ConfigValuesIterator iter(target_); !iter.done(); iter.Next()) {
    const ConfigValues& cur = iter.cur();
    for (const auto& [crate_name, lib_file] : cur.externs()) {
      write_extern_lib_file(crate_name, lib_file);
    }
  }

  out_ << std::endl;
  out_ << "  rustdeps =";

  for (const SourceDir& dir : private_extern_dirs) {
    // TODO: switch to using `--extern priv:name` after stabilization.
    out_ << " -Ldependency=";
    path_output_.WriteDir(out_, dir, PathOutput::DIR_NO_LAST_SLASH);
  }

  // Non-Rust native dependencies.
  UniqueVector<SourceDir> nonrustdep_dirs;
  for (const auto& nonrustdep : nonrustdeps) {
    nonrustdep_dirs.push_back(
        nonrustdep.AsSourceFile(settings_->build_settings()).GetDir());
  }
  // First -Lnative to specify the search directories.
  // This is necessary for #[link(...)] directives to work properly.
  for (const auto& nonrustdep_dir : nonrustdep_dirs) {
    out_ << " -Lnative=";
    path_output_.WriteDir(out_, nonrustdep_dir, PathOutput::DIR_NO_LAST_SLASH);
  }
  // Before outputting any libraries to link, ensure the linker is in a mode
  // that allows dynamic linking, as rustc may have previously put it into
  // static-only mode.
  if (nonrustdeps.size() > 0) {
    out_ << " -Clink-arg=-Bdynamic";
  }
  for (const auto& nonrustdep : nonrustdeps) {
    out_ << " -Clink-arg=";
    path_output_.WriteFile(out_, nonrustdep);
  }
  WriteLibrarySearchPath(out_, tool_);
  WriteLibs(out_, tool_);
  out_ << std::endl;
  out_ << "  ldflags =";
  WriteCustomLinkerFlags(out_, tool_);
  out_ << std::endl;
}

void NinjaTargetWriter::ClassifyDependency(
    const Target* dep,
    ClassifiedDeps* classified_deps) const {
  // Only the following types of outputs have libraries linked into them:
  //  EXECUTABLE
  //  SHARED_LIBRARY
  //  _complete_ STATIC_LIBRARY
  //
  // Child deps of intermediate static libraries get pushed up the
  // dependency tree until one of these is reached, and source sets
  // don't link at all.
  bool can_link_libs = target_->IsFinal();

  if (can_link_libs && dep->builds_swift_module())
    classified_deps->swiftmodule_deps.push_back(dep);

  if (target_->source_types_used().RustSourceUsed() &&
      (target_->output_type() == Target::RUST_LIBRARY ||
       target_->output_type() == Target::STATIC_LIBRARY) &&
      dep->IsLinkable()) {
    // Rust libraries and static libraries aren't final, but need to have the
    // link lines of all transitive deps specified.
    classified_deps->linkable_deps.push_back(dep);
  } else if (dep->output_type() == Target::SOURCE_SET ||
             // If a complete static library depends on an incomplete static
             // library, manually link in the object files of the dependent
             // library as if it were a source set. This avoids problems with
             // braindead tools such as ar which don't properly link dependent
             // static libraries.
             (target_->complete_static_lib() &&
              (dep->output_type() == Target::STATIC_LIBRARY &&
               !dep->complete_static_lib()))) {
    // Source sets have their object files linked into final targets
    // (shared libraries, executables, loadable modules, and complete static
    // libraries). Intermediate static libraries and other source sets
    // just forward the dependency, otherwise the files in the source
    // set can easily get linked more than once which will cause
    // multiple definition errors.
    if (can_link_libs)
      AddSourceSetFiles(dep, &classified_deps->extra_object_files);

    // Add the source set itself as a non-linkable dependency on the current
    // target. This will make sure that anything the source set's stamp file
    // depends on (like data deps) are also built before the current target
    // can be complete. Otherwise, these will be skipped since this target
    // will depend only on the source set's object files.
    classified_deps->non_linkable_deps.push_back(dep);
  } else if (target_->complete_static_lib() && dep->IsFinal()) {
    classified_deps->non_linkable_deps.push_back(dep);
  } else if (can_link_libs && dep->IsLinkable()) {
    classified_deps->linkable_deps.push_back(dep);
  } else if (dep->output_type() == Target::CREATE_BUNDLE &&
             dep->bundle_data().is_framework()) {
    classified_deps->framework_deps.push_back(dep);
  } else {
    classified_deps->non_linkable_deps.push_back(dep);
  }
}
