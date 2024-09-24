// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/target_flattener.h"

#include <stddef.h>
#include <iostream>
#include <memory>
#include <utility>

#include "gn/action_target_generator.h"
#include "gn/binary_target_generator.h"
#include "gn/build_settings.h"
#include "gn/bundle_data_target_generator.h"
#include "gn/config.h"
#include "gn/copy_target_generator.h"
#include "gn/create_bundle_target_generator.h"
#include "gn/err.h"
#include "gn/filesystem_utils.h"
#include "gn/functions.h"
#include "gn/generated_file_target_generator.h"
#include "gn/group_target_generator.h"
#include "gn/metadata.h"
#include "gn/parse_tree.h"
#include "gn/scheduler.h"
#include "gn/scope.h"
#include "gn/token.h"
#include "gn/value.h"
#include "gn/value_extractors.h"
#include "gn/variables.h"
#include "gn/deps_iterator.h"


// static 
void TargetFlattener::FlattenTarget(Target* target, Err* err) {
  if(target->ShouldFormatFlattenDeps()) {
    auto flatten_deps = target->flatten_deps();
    for (const auto& flatten_dep : flatten_deps) {
      TargetFlattener(target, flatten_dep.ptr, err).FlattenTargets();
    }
  }
}

TargetFlattener::TargetFlattener(Target* target, const Target* flatten_target, Err* err):target_(target), flatten_targets_(flatten_target), err_(err) {}

void TargetFlattener::Flatten() {
  // All target types use these.
  if (!FlattenDependentConfigs())
    return;

  if (!FlattenData())
    return;

  if (!FlattenDependencies())
    return;

  if (!FlattenMetadata())
    return;
  if (!FlattenAssertNoDeps())
    return;

  if (!FlattenWriteRuntimeDeps())
    return;

  if (!FlattenSources()) {
    return;
  }
  
  if(!FlattenConfigs()) {
    return;
  }
}

void TargetFlattener::FlattenTargets() {
  auto target_out_type = target_->output_type();
  auto should_flatten = target_out_type == Target::EXECUTABLE ||
                        target_out_type == Target::GROUP ||
                        target_out_type == Target::SHARED_LIBRARY ||
                        target_out_type == Target::STATIC_LIBRARY ||
                        target_out_type == Target::SOURCE_SET;
  if (!should_flatten) {
    std::cout<< "不支持此类型的target使用flatten_deps"<< std::endl;
    return;
  }
  auto flatten_targets_out_type = flatten_targets_->output_type();
  if (target_out_type != flatten_targets_out_type) {
    // *err = Err(flatten_targets_out_type, "Target generator requires one string argument.",
    //            "不能不同类型的gn target 使用flatten_deps");
    std::cout<< "不能不同类型的gn target 使用flatten_deps"<< std::endl;
    return;
  }


  Flatten();

  if (target_out_type == Target::EXECUTABLE) {

    FlattenPublic();
    FlattenAllowCircularIncludesFrom();
    FlattenConfigValues();
  } else if (target_out_type == Target::GROUP) {
  } else if (target_out_type == Target::SHARED_LIBRARY) {
    FlattenPublic();
    FlattenAllowCircularIncludesFrom();
    FlattenConfigValues();
  } else if (target_out_type == Target::STATIC_LIBRARY) {
    FlattenPublic();
    FlattenAllowCircularIncludesFrom();
    FlattenConfigValues();
  } else if (target_out_type == Target::SOURCE_SET) {
    FlattenPublic();
    FlattenAllowCircularIncludesFrom();
    FlattenConfigValues();
  } else {
    std::cout<< "Not a known target type " <<std::endl;
    // *err = Err(function_call, "Not a known target type",
    //            "只支持上述类型使用flatten_deps");
  }

  // if (err->has_error())
  target_->MarkFormatedFlattenDeps();
  return;
}

bool TargetFlattener::FlattenSources() {
  auto& target_sources_ = target_->sources();
  auto& flatten_targets_sources_ = flatten_targets_->sources();
  // for (auto& files : target_sources_) {
  //   std::cout<<  "target_sources_" <<target_->label().GetUserVisibleName(false) << " has file:" << files.GetName()<< std::endl;
  // }
  // for (auto& files_ : flatten_targets_sources_) {
  //   std::cout<< "flatten_targets_sources_ " <<flatten_targets_->label().GetUserVisibleName(false) << " has file:" << files_.GetName()<< std::endl;
  // }
  target_sources_.insert(target_sources_.end(), flatten_targets_sources_.begin(), flatten_targets_sources_.end());
  // target_->sources() = target_sources_;
  // for (auto& files : target_->sources()) {
  //   std::cout<<  "target_sources22_" <<target_->label().GetUserVisibleName(false) << " has file:" << files.GetName()<< std::endl;
  // }
  return true;
}

bool TargetFlattener::FlattenPublic() {
  auto& to_public_headers_ = target_->public_headers();
  auto& from_public_headers_ = flatten_targets_->public_headers();
  to_public_headers_.insert(to_public_headers_.end(), from_public_headers_.begin(), from_public_headers_.end());
  target_->public_headers() = to_public_headers_;
  return true;
}

bool TargetFlattener::FlattenFriends() {
  auto& to_friends_ = target_->friends();
  auto& from_friends_ = flatten_targets_->friends();
  to_friends_.insert(to_friends_.end(), from_friends_.begin(), from_friends_.end());
  return true;
}

bool TargetFlattener::FlattenAllowCircularIncludesFrom() {
  auto& circular = flatten_targets_->allow_circular_includes_from();

  // Validate that all circular includes entries are in the deps.
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
      *err_ = Err(Value(), "Label not in deps.",
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
  auto& from_config_values = flatten_targets_->config_values();
  to_config_values.AppendValues(from_config_values);
  return true;
}

bool TargetFlattener::FlattenConfigs() {
  std::cout<< "FlattenConfigs flatten_targets_ name: "<< flatten_targets_->label().GetUserVisibleName(false) << std::endl;
  for (auto& label_config : flatten_targets_->configs()) {
    std::cout<< "flatten_targets_ configs: "<< label_config.label.GetUserVisibleName(false) << std::endl;
  }
  return FlattenGenericConfigs(&flatten_targets_->configs(), &target_->configs());
}

bool TargetFlattener::FlattenDependentConfigs() {
  if (!FlattenGenericConfigs(&flatten_targets_->all_dependent_configs(),
                          &target_->all_dependent_configs()))
    return false;

  if (!FlattenGenericConfigs(&flatten_targets_->public_configs(),
                          &target_->public_configs()))
    return false;

  return true;
}

bool TargetFlattener::FlattenData() {
  auto& output_list = target_->data();
  auto& flatten_output_list = flatten_targets_->data();
  output_list.insert(output_list.end(), flatten_output_list.begin(), flatten_output_list.end());
  return true;
}

bool TargetFlattener::FlattenDependencies() {
  if (!FlattenGenericDeps(&flatten_targets_->private_deps(), &target_->private_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_targets_->public_deps(), &target_->public_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_targets_->data_deps(), &target_->data_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_targets_->gen_deps(), &target_->gen_deps()))
    return false;
  if (!FlattenGenericDeps(&flatten_targets_->data_deps(), &target_->data_deps()))
    return false;

  return true;
}

bool TargetFlattener::FlattenMetadata() {
  auto& to_matadata = target_->metadata();
  auto& to_matadata_content = to_matadata.contents();
  auto& from_matadata = flatten_targets_->metadata();
  auto from_matadata_content = from_matadata.contents();
  to_matadata_content.merge(from_matadata_content);
  return true;
}

bool TargetFlattener::FlattenAssertNoDeps() {
  auto& from_assert_no_deps = flatten_targets_->assert_no_deps();
  auto& to_assert_no_deps = target_->assert_no_deps();
  to_assert_no_deps.insert(to_assert_no_deps.end(), from_assert_no_deps.begin(), from_assert_no_deps.end());
  return true;
}

bool TargetFlattener::FillOutputs(bool allow_substitutions) {
  // const Value* value = scope_->GetValue(variables::kOutputs, true);
  // if (!value)
  //   return true;

  // SubstitutionList& outputs = target_->action_values().outputs();
  // if (!outputs.Parse(*value, err_))
  //   return false;

  // if (!allow_substitutions) {
  //   // Verify no substitutions were actually used.
  //   if (!outputs.required_types().empty()) {
  //     *err_ =
  //         Err(*value, "Source expansions not allowed here.",
  //             "The outputs of this target used source {{expansions}} but this "
  //             "target type\ndoesn't support them. Just express the outputs "
  //             "literally.");
  //     return false;
  //   }
  // }

  // // Check the substitutions used are valid for this purpose.
  // if (!EnsureValidSubstitutions(outputs.required_types(),
  //                               &IsValidSourceSubstitution, value->origin(),
  //                               err_))
  //   return false;

  // // Validate that outputs are in the output dir.
  // CHECK(outputs.list().size() == value->list_value().size());
  // for (size_t i = 0; i < outputs.list().size(); i++) {
  //   if (!EnsureSubstitutionIsInOutputDir(outputs.list()[i],
  //                                        value->list_value()[i]))
  //     return false;
  // }
  return true;
}

bool TargetFlattener::FillCheckIncludes() {
  // const Value* value = scope_->GetValue(variables::kCheckIncludes, true);
  // if (!value)
  //   return true;
  // if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
  //   return false;
  // target_->set_check_includes(value->boolean_value());
  return true;
}

bool TargetFlattener::FillOutputExtension() {
  // const Value* value = scope_->GetValue(variables::kOutputExtension, true);
  // if (!value)
  //   return true;
  // if (!value->VerifyTypeIs(Value::STRING, err_))
  //   return false;
  // target_->set_output_extension(value->string_value());
  return true;
}

bool TargetFlattener::EnsureSubstitutionIsInOutputDir(
    const SubstitutionPattern& pattern,
    const Value& original_value) {
  if (pattern.ranges().empty()) {
    // Pattern is empty, error out (this prevents weirdness below).
    *err_ = Err(original_value, "This has an empty value in it.");
    return false;
  }

  if (pattern.ranges()[0].type == &SubstitutionLiteral) {
    // If the first thing is a literal, it must start with the output dir.
    if (!EnsureStringIsInOutputDir(GetBuildSettings()->build_dir(),
                                   pattern.ranges()[0].literal,
                                   original_value.origin(), err_))
      return false;
  } else {
    // Otherwise, the first subrange must be a pattern that expands to
    // something in the output directory.
    if (!SubstitutionIsInOutputDir(pattern.ranges()[0].type)) {
      *err_ =
          Err(original_value, "File is not inside output directory.",
              "The given file should be in the output directory. Normally you\n"
              "would specify\n\"$target_out_dir/foo\" or "
              "\"{{source_gen_dir}}/foo\".");
      return false;
    }
  }

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

// 这个不用合并
bool TargetFlattener::FlattenWriteRuntimeDeps() {
  auto output_file = target_->write_runtime_deps_output();
  auto flatten_output_file = flatten_targets_->write_runtime_deps_output();
  std::cout << "output_file" << output_file.value() << std::endl;
  std::cout << "flatten_output_file" << flatten_output_file.value() << std::endl;

  return true;
}
