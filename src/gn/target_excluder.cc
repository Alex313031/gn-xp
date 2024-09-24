// Copyright (c) 2024 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/target_excluder.h"

#include "gn/value_extractors.h"

// static 
bool TargetExcluder::ExcluderTarget(Target* target, Err* err) {
  return TargetExcluder(target, err).ExcluderTarget();
}

TargetExcluder::TargetExcluder(Target* target, Err* err):target_(target), err_(err) {}

bool TargetExcluder::Excluder() {
  // All target types use these.
  if (!ExcludeDependentConfigs())
    return false;

  if (!ExcludeData())
    return false;

  if (!ExcludeDependencies())
    return false;

  if (!ExcludeMetadata())
    return false;
  if (!ExcludeAssertNoDeps())
    return false;

  if (!ExcludeWriteRuntimeDeps())
    return false;

  if (!ExcludeSources()) {
    return false;
  }
  
  if(!ExcludeConfigs()) {
    return false;
  }
  return true;
}

bool TargetExcluder::ExcluderTarget() {
  auto target_out_type = target_->output_type();
  auto should_exclude = target_out_type == Target::EXECUTABLE ||
                        target_out_type == Target::GROUP ||
                        target_out_type == Target::SHARED_LIBRARY ||
                        target_out_type == Target::STATIC_LIBRARY ||
                        target_out_type == Target::SOURCE_SET;
  if (!should_exclude) {
   *err_ = Err(target_->defined_from(), "The target of this type is not supported by using flatten_deps.",
               "flatten_deps is available only for executable, group, shared_library, static_library, source_set.");
    return false;
  }
  Excluder();

  if (target_out_type != Target::GROUP) {
    ExcludePublic();
    ExcludeAllowCircularIncludesFrom();
    ExcludeConfigValues();
    ExcludeFriends();
  }

  if (err_->has_error())
    return false;
  return true;
}

bool TargetExcluder::ExcludeSources() {
  auto& target_sources_ = target_->sources();
  auto& exclude_sources = target_->exclude_sources();
  VectorExclude(&target_sources_, exclude_sources);
  target_->source_types_used().reset();
  for (std::size_t i = 0; i < target_sources_.size(); ++i) {
    const auto& source = target_sources_[i];
    const SourceFile::Type source_type = source.GetType();
    switch (source_type) {
      case SourceFile::SOURCE_CPP:
      case SourceFile::SOURCE_MODULEMAP:
      case SourceFile::SOURCE_H:
      case SourceFile::SOURCE_C:
      case SourceFile::SOURCE_M:
      case SourceFile::SOURCE_MM:
      case SourceFile::SOURCE_S:
      case SourceFile::SOURCE_ASM:
      case SourceFile::SOURCE_O:
      case SourceFile::SOURCE_DEF:
      case SourceFile::SOURCE_GO:
      case SourceFile::SOURCE_RS:
      case SourceFile::SOURCE_RC:
      case SourceFile::SOURCE_SWIFT:
        // These are allowed.
        break;
      case SourceFile::SOURCE_UNKNOWN:
      case SourceFile::SOURCE_SWIFTMODULE:
      case SourceFile::SOURCE_NUMTYPES:
        *err_ = Err(target_->defined_from(),
                std::string("Only source, header, and object files belong in "
                            "the sources of a ") +
                    Target::GetStringForOutputType(target_->output_type()) +
                    ". " + source.value() + " is not one of the valid types.");
    }

    target_->source_types_used().Set(source_type);
  }
  if (err_->has_error())
    return false;
  return true;
}

bool TargetExcluder::ExcludePublic() {
  auto& to_public_headers_ = target_->public_headers();
  auto& exclude_public_headers = target_->exclude_public_headers();
  VectorExclude(&to_public_headers_, exclude_public_headers);
  return true;
}

bool TargetExcluder::ExcludeFriends() {
  auto& to_friends_ = target_->friends();
  auto& exclude_friends = target_->exclude_friends();
  VectorExclude(&to_friends_, exclude_friends);
  return true;
}

bool TargetExcluder::ExcludeAllowCircularIncludesFrom() {
  return true;
}

bool TargetExcluder::ExcludeConfigValues() {
  auto& to_config_values = target_->config_values();
  auto& exclude_config_values = target_->exclude_config_values();
  to_config_values.ExcludeValues(exclude_config_values);
  return true;
}

bool TargetExcluder::ExcludeConfigs() {
  if (!ExcludeGenericConfigs(&target_->exclude_configs(), &target_->configs())) {
    return false;
  }
  if (!ExcludeGenericConfigs(&target_->exclude_all_dependent_configs(), &target_->configs())) {
    return false;
  }
  if (!ExcludeGenericConfigs(&target_->exclude_public_configs(), &target_->configs())) {
    return false;
  }
  return true;
}

bool TargetExcluder::ExcludeDependentConfigs() {
  if (!ExcludeGenericConfigs(&target_->exclude_all_dependent_configs(), &target_->all_dependent_configs())) {
    return false;
  }
  if (!ExcludeGenericConfigs(&target_->exclude_public_configs(), &target_->public_configs())) {
    return false;
  }

  return true;
}

bool TargetExcluder::ExcludeData() {
  auto& output_list = target_->data();
  auto& exclude_output_list = target_->exclude_data();
  VectorExclude(&output_list, exclude_output_list);
  return true;
}

bool TargetExcluder::ExcludeDependencies() {
  if (!ExcludeGenericDeps(&target_->exclude_private_deps(), &target_->private_deps()))
    return false;
  if (!ExcludeGenericDeps(&target_->exclude_public_deps(), &target_->public_deps()))
    return false;
  if (!ExcludeGenericDeps(&target_->exclude_data_deps(), &target_->data_deps()))
    return false;
  if (!ExcludeGenericDeps(&target_->exclude_gen_deps(), &target_->gen_deps()))
    return false;

  return true;
}

bool TargetExcluder::ExcludeMetadata() {
  return true;
}

bool TargetExcluder::ExcludeAssertNoDeps() {
  auto& to_assert_no_deps = target_->assert_no_deps();
  VectorExclude(&to_assert_no_deps, target_->exclude_assert_no_deps());
  return true;
}

bool TargetExcluder::ExcludeGenericConfigs(const UniqueVector<LabelConfigPair>* exclude,
                            UniqueVector<LabelConfigPair>* dest) {
  VectorExclude(dest, *exclude);
  return true;
}
bool TargetExcluder::ExcludeGenericDeps(const LabelTargetVector* exclude,
                        LabelTargetVector* dest) {
  VectorExclude(dest, *exclude);
  return true;
}

bool TargetExcluder::ExcludeWriteRuntimeDeps() {
  return true;
}
