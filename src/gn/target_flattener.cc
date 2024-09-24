// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/target_flattener.h"

#include "gn/deps_iterator.h"

// static 
bool TargetFlattener::FlattenTarget(Target* target, Err* err) {
  if(target->ShouldFormatFlattenDeps()) {
    auto flatten_deps = target->flatten_deps();
    for (const auto& flatten_dep : flatten_deps) {
      if (!TargetFlattener(target, &flatten_dep, err).FlattenTarget())
        return false;
    }
    target->MarkFormatedFlattenDeps();
  }
  return true;
}

TargetFlattener::TargetFlattener(Target* target, const LabelTargetPair* flatten_dep, Err* err): target_(target), flatten_dep_pair_(flatten_dep), err_(err) {
  flatten_dep_ = flatten_dep->ptr;
}

bool TargetFlattener::FlattenCommon() {
  // flatten configs, public_configs and all_dependent_configs
  if (!FlattenAllConfigs())
    return false;
  // flatten data
  if (!FlattenData())
    return false;
  // flatten deps, public_deps, data_deps and gen_deps
  if (!FlattenDependencies())
    return false;
  // flatten metadata
  if (!FlattenMetadata())
    return false;
  // flatten assert_no_deps
  if (!FlattenAssertNoDeps())
    return false;
  // flatten sources 
  if (!FlattenSources())
    return false;
  return true;
}

bool TargetFlattener::FlattenTarget() {
  auto target_out_type = target_->output_type();
  auto should_flatten = target_out_type == Target::EXECUTABLE ||
                        target_out_type == Target::GROUP ||
                        target_out_type == Target::SHARED_LIBRARY ||
                        target_out_type == Target::STATIC_LIBRARY ||
                        target_out_type == Target::SOURCE_SET;
  if (!should_flatten) {
   *err_ = Err(target_->defined_from(), "The target of this type is not supported by using flatten_deps.",
               "flatten_deps is available only for executable, group, shared_library, static_library, source_set.");
    return false;
  }
  auto flatten_dep_out_type = flatten_dep_->output_type();
  if (target_out_type != flatten_dep_out_type) {
    std::string msg = flatten_dep_->output_name() + "'s type is not equal to" + target_->output_name() + "'s type.";
   *err_ = Err(flatten_dep_pair_->origin, msg, "");
    return false;
  }
  FlattenCommon();

  if (target_out_type != Target::GROUP) {
    // flatten public_headers
    FlattenPublic();
    // flatten friends
    FlattenAllowCircularIncludesFrom();
    FlattenConfigValues();
    FlattenFriends();
  }


  if (err_->has_error())
    return false;
  return true;
}

bool TargetFlattener::FlattenSources() {
  auto& target_sources_ = target_->sources();
  auto& flatten_dep_sources_ = flatten_dep_->sources();
  target_sources_.insert(target_sources_.end(), flatten_dep_sources_.begin(), flatten_dep_sources_.end());
  return true;
}

bool TargetFlattener::FlattenPublic() {
  auto& to_public_headers_ = target_->public_headers();
  auto& from_public_headers_ = flatten_dep_->public_headers();
  to_public_headers_.insert(to_public_headers_.end(), from_public_headers_.begin(), from_public_headers_.end());
  target_->public_headers() = to_public_headers_;
  return true;
}

bool TargetFlattener::FlattenFriends() {
  auto& to_friends_ = target_->friends();
  auto& from_friends_ = flatten_dep_->friends();
  to_friends_.insert(to_friends_.end(), from_friends_.begin(), from_friends_.end());
  return true;
}

bool TargetFlattener::FlattenAllowCircularIncludesFrom() {
  auto& circular = flatten_dep_->allow_circular_includes_from();

  // Validate that all circular includes entries are in the flatten deps.
  for (const auto& cur : circular) {
    bool found_dep = false;
    for (const auto& dep_pair : target_->GetDeps(Target::DEPS_LINKED)) {
      if (dep_pair.label == cur) {
        found_dep = true;
        break;
      }
    }
    if (!found_dep) {
      bool with_toolchain = target_->toolchain();
      *err_ = Err(flatten_dep_pair_->origin, "Label not in deps.",
                  "The label \"" + cur.GetUserVisibleName(with_toolchain) +
                      "\"\nwas not in the deps of this target. "
                      "allow_circular_includes_from only allows\ntargets "
                      "present in the "
                      "deps.");
      return false;
    }
  }

  // Add to the set.
  for (const auto& cur : circular)
    target_->allow_circular_includes_from().insert(cur);
  return true;
}

bool TargetFlattener::FlattenConfigValues() {
  auto& to_config_values = target_->config_values();
  auto& from_config_values = flatten_dep_->config_values();
  to_config_values.AppendValues(from_config_values);
  return true;
}

bool TargetFlattener::FlattenAllConfigs() {
  if (!FlattenGenericConfigs(&flatten_dep_->configs(),
                          &target_->configs()))
    return false;
  if (!FlattenGenericConfigs(&flatten_dep_->all_dependent_configs(),
                          &target_->all_dependent_configs()))
    return false;
  if (!FlattenGenericConfigs(&flatten_dep_->public_configs(),
                          &target_->public_configs()))
    return false;
  return true;
}

bool TargetFlattener::FlattenData() {
  auto& output_list = target_->data();
  auto& flatten_output_list = flatten_dep_->data();
  output_list.insert(output_list.end(), flatten_output_list.begin(), flatten_output_list.end());
  return true;
}

bool TargetFlattener::FlattenDependencies() {
  if (!FlattenGenericDeps(&flatten_dep_->private_deps(), &target_->private_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_dep_->public_deps(), &target_->public_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_dep_->data_deps(), &target_->data_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_dep_->gen_deps(), &target_->gen_deps()))
    return false;

  return true;
}

bool TargetFlattener::FlattenMetadata() {
  auto& to_matadata_content = target_->metadata().contents();
  auto from_matadata_content = flatten_dep_->metadata().contents();
  to_matadata_content.merge(from_matadata_content);
  return true;
}

bool TargetFlattener::FlattenAssertNoDeps() {
  auto& from_assert_no_deps = flatten_dep_->assert_no_deps();
  auto& to_assert_no_deps = target_->assert_no_deps();
  to_assert_no_deps.insert(to_assert_no_deps.end(), from_assert_no_deps.begin(), from_assert_no_deps.end());
  return true;
}
bool TargetFlattener::FlattenGenericConfigs(const UniqueVector<LabelConfigPair>* from,
                                         UniqueVector<LabelConfigPair>* dest) {
  dest->reserve(from->size() + dest->size());
  dest->Append(*from);
  return true;
}

bool TargetFlattener::FlattenGenericDeps(const LabelTargetVector* from,
                                      LabelTargetVector* dest) {
  dest->insert(dest->end(), from->begin(), from->end());
  return true;
}
