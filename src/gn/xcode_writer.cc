// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/xcode_writer.h"

#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include "base/environment.h"
#include "base/logging.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "gn/args.h"
#include "gn/build_settings.h"
#include "gn/builder.h"
#include "gn/commands.h"
#include "gn/deps_iterator.h"
#include "gn/filesystem_utils.h"
#include "gn/settings.h"
#include "gn/source_file.h"
#include "gn/target.h"
#include "gn/value.h"
#include "gn/variables.h"
#include "gn/xcode_object.h"

#include <iostream>

namespace {

using TargetToFileList = std::unordered_map<const Target*, Target::FileList>;
using TargetToTarget = std::unordered_map<const Target*, const Target*>;
using TargetToPBXTarget = std::unordered_map<const Target*, PBXTarget*>;

enum TargetOsType {
  WRITER_TARGET_OS_IOS,
  WRITER_TARGET_OS_MACOS,
};

const char* kXCTestFileSuffixes[] = {
    "egtest.m",
    "egtest.mm",
    "xctest.m",
    "xctest.mm",
};

const char kXCTestModuleTargetNamePostfix[] = "_module";
const char kXCUITestRunnerTargetNamePostfix[] = "_runner";

struct SafeEnvironmentVariableInfo {
  const char* name;
  bool capture_at_generation;
};

SafeEnvironmentVariableInfo kSafeEnvironmentVariables[] = {
    {"HOME", true},
    {"LANG", true},
    {"PATH", true},
    {"USER", true},
    {"TMPDIR", false},
    {"ICECC_VERSION", true},
    {"ICECC_CLANG_REMOTE_CPP", true}};

TargetOsType GetTargetOs(const Args& args) {
  const Value* target_os_value = args.GetArgOverride(variables::kTargetOs);
  if (target_os_value) {
    if (target_os_value->type() == Value::STRING) {
      if (target_os_value->string_value() == "ios")
        return WRITER_TARGET_OS_IOS;
    }
  }
  return WRITER_TARGET_OS_MACOS;
}

std::string GetNinjaExecutable(const std::string& ninja_executable) {
  return ninja_executable.empty() ? "ninja" : ninja_executable;
}

std::string GetBuildScript(const std::string& target_name,
                           const std::string& ninja_executable,
                           const std::string& ninja_extra_args,
                           base::Environment* environment) {
  std::stringstream script;
  script << "echo note: \"Compile and copy " << target_name << " via ninja\"\n"
         << "exec ";

  // Launch ninja with a sanitized environment (Xcode sets many environment
  // variable overridding settings, including the SDK, thus breaking hermetic
  // build).
  script << "env -i ";
  for (const auto& variable : kSafeEnvironmentVariables) {
    script << variable.name << "=\"";

    std::string value;
    if (variable.capture_at_generation)
      environment->GetVar(variable.name, &value);

    if (!value.empty())
      script << value;
    else
      script << "$" << variable.name;
    script << "\" ";
  }

  script << GetNinjaExecutable(ninja_executable) << " -C .";
  if (!ninja_extra_args.empty())
    script << " " << ninja_extra_args;
  if (!target_name.empty())
    script << " " << target_name;
  script << "\nexit 1\n";
  return script.str();
}

bool IsApplicationTarget(const Target* target) {
  return target->output_type() == Target::CREATE_BUNDLE &&
         target->bundle_data().product_type() ==
             "com.apple.product-type.application";
}

bool IsXCUITestRunnerTarget(const Target* target) {
  return IsApplicationTarget(target) &&
         base::EndsWith(target->label().name(),
                        kXCUITestRunnerTargetNamePostfix,
                        base::CompareCase::SENSITIVE);
}

bool IsXCTestModuleTarget(const Target* target) {
  return target->output_type() == Target::CREATE_BUNDLE &&
         target->bundle_data().product_type() ==
             "com.apple.product-type.bundle.unit-test" &&
         base::EndsWith(target->label().name(), kXCTestModuleTargetNamePostfix,
                        base::CompareCase::SENSITIVE);
}

bool IsXCUITestModuleTarget(const Target* target) {
  return target->output_type() == Target::CREATE_BUNDLE &&
         target->bundle_data().product_type() ==
             "com.apple.product-type.bundle.ui-testing" &&
         base::EndsWith(target->label().name(), kXCTestModuleTargetNamePostfix,
                        base::CompareCase::SENSITIVE);
}

bool IsXCTestFile(const SourceFile& file) {
  std::string file_name = file.GetName();
  for (size_t i = 0; i < std::size(kXCTestFileSuffixes); ++i) {
    if (base::EndsWith(file_name, kXCTestFileSuffixes[i],
                       base::CompareCase::SENSITIVE)) {
      return true;
    }
  }

  return false;
}

const Target* FindApplicationTargetByName(
    const ParseNode* node,
    const std::string& target_name,
    const std::vector<const Target*>& targets,
    Err* err) {
  for (const Target* target : targets) {
    if (target->label().name() == target_name) {
      if (!IsApplicationTarget(target)) {
        *err = Err(node, "host application target \"" + target_name +
                             "\" not an application bundle");
        return nullptr;
      }
      return target;
    }
  }
  *err =
      Err(node, "cannot find host application bundle \"" + target_name + "\"");
  return nullptr;
}

// Adds |base_pbxtarget| as a dependency of |dependent_pbxtarget| in the
// generated Xcode project.
void AddPBXTargetDependency(const PBXTarget* base_pbxtarget,
                            PBXTarget* dependent_pbxtarget,
                            const PBXProject* project) {
  auto container_item_proxy =
      std::make_unique<PBXContainerItemProxy>(project, base_pbxtarget);
  auto dependency = std::make_unique<PBXTargetDependency>(
      base_pbxtarget, std::move(container_item_proxy));

  dependent_pbxtarget->AddDependency(std::move(dependency));
}

// Adds the corresponding test application target as dependency of xctest or
// xcuitest module target in the generated Xcode project.
bool AddDependencyTargetForTestModuleTargets(
    const std::vector<const Target*>& targets,
    const TargetToPBXTarget& bundle_target_to_pbxtarget,
    const PBXProject* project,
    Err* err) {
  for (const Target* target : targets) {
    if (!IsXCTestModuleTarget(target) && !IsXCUITestModuleTarget(target))
      continue;

    const Target* test_application_target = FindApplicationTargetByName(
        target->defined_from(),
        target->bundle_data().xcode_test_application_name(), targets, err);
    if (!test_application_target)
      return false;

    const PBXTarget* test_application_pbxtarget =
        bundle_target_to_pbxtarget.at(test_application_target);
    PBXTarget* module_pbxtarget = bundle_target_to_pbxtarget.at(target);
    DCHECK(test_application_pbxtarget);
    DCHECK(module_pbxtarget);

    AddPBXTargetDependency(test_application_pbxtarget, module_pbxtarget,
                           project);
  }

  return true;
}

// Searches the list of xctest files recursively under |target|.
void SearchXCTestFilesForTarget(const Target* target,
                                TargetToFileList* xctest_files_per_target) {
  // Early return if already visited and processed.
  if (xctest_files_per_target->find(target) != xctest_files_per_target->end())
    return;

  Target::FileList xctest_files;
  for (const SourceFile& file : target->sources()) {
    if (IsXCTestFile(file)) {
      xctest_files.push_back(file);
    }
  }

  // Call recursively on public and private deps.
  for (const auto& t : target->public_deps()) {
    SearchXCTestFilesForTarget(t.ptr, xctest_files_per_target);
    const Target::FileList& deps_xctest_files =
        (*xctest_files_per_target)[t.ptr];
    xctest_files.insert(xctest_files.end(), deps_xctest_files.begin(),
                        deps_xctest_files.end());
  }

  for (const auto& t : target->private_deps()) {
    SearchXCTestFilesForTarget(t.ptr, xctest_files_per_target);
    const Target::FileList& deps_xctest_files =
        (*xctest_files_per_target)[t.ptr];
    xctest_files.insert(xctest_files.end(), deps_xctest_files.begin(),
                        deps_xctest_files.end());
  }

  // Sort xctest_files to remove duplicates.
  std::sort(xctest_files.begin(), xctest_files.end());
  xctest_files.erase(std::unique(xctest_files.begin(), xctest_files.end()),
                     xctest_files.end());

  xctest_files_per_target->insert(std::make_pair(target, xctest_files));
}

// Add xctest files to the "Compiler Sources" of corresponding test module
// native targets.
void AddXCTestFilesToTestModuleTarget(const Target::FileList& xctest_file_list,
                                      PBXNativeTarget* native_target,
                                      PBXProject* project,
                                      SourceDir source_dir,
                                      const BuildSettings* build_settings) {
  for (const SourceFile& source : xctest_file_list) {
    std::string source_path = RebasePath(source.value(), source_dir,
                                         build_settings->root_path_utf8());

    // Test files need to be known to Xcode for proper indexing and for
    // discovery of tests function for XCTest and XCUITest, but the compilation
    // is done via ninja and thus must prevent Xcode from compiling the files by
    // adding '-help' as per file compiler flag.
    project->AddSourceFile(source_path, source_path, CompilerFlags::HELP,
                           native_target);
  }
}

class CollectPBXObjectsPerClassHelper : public PBXObjectVisitorConst {
 public:
  CollectPBXObjectsPerClassHelper() = default;

  void Visit(const PBXObject* object) override {
    DCHECK(object);
    objects_per_class_[object->Class()].push_back(object);
  }

  const std::map<PBXObjectClass, std::vector<const PBXObject*>>&
  objects_per_class() const {
    return objects_per_class_;
  }

 private:
  std::map<PBXObjectClass, std::vector<const PBXObject*>> objects_per_class_;

  DISALLOW_COPY_AND_ASSIGN(CollectPBXObjectsPerClassHelper);
};

std::map<PBXObjectClass, std::vector<const PBXObject*>>
CollectPBXObjectsPerClass(const PBXProject* project) {
  CollectPBXObjectsPerClassHelper visitor;
  project->Visit(visitor);
  return visitor.objects_per_class();
}

class RecursivelyAssignIdsHelper : public PBXObjectVisitor {
 public:
  RecursivelyAssignIdsHelper(const std::string& seed)
      : seed_(seed), counter_(0) {}

  void Visit(PBXObject* object) override {
    std::stringstream buffer;
    buffer << seed_ << " " << object->Name() << " " << counter_;
    std::string hash = base::SHA1HashString(buffer.str());
    DCHECK_EQ(hash.size() % 4, 0u);

    uint32_t id[3] = {0, 0, 0};
    const uint32_t* ptr = reinterpret_cast<const uint32_t*>(hash.data());
    for (size_t i = 0; i < hash.size() / 4; i++)
      id[i % 3] ^= ptr[i];

    object->SetId(base::HexEncode(id, sizeof(id)));
    ++counter_;
  }

 private:
  std::string seed_;
  int64_t counter_;

  DISALLOW_COPY_AND_ASSIGN(RecursivelyAssignIdsHelper);
};

void RecursivelyAssignIds(PBXProject* project) {
  RecursivelyAssignIdsHelper visitor(project->Name());
  project->Visit(visitor);
}

// Returns a configuration name derived from the build directory. This gives
// standard names if using the Xcode convention of naming the build directory
// out/$configuration-$platform (e.g. out/Debug-iphonesimulator).
std::string ConfigNameFromBuildSettings(const BuildSettings* build_settings) {
  std::string config_name = FilePathToUTF8(build_settings->build_dir()
                                               .Resolve(base::FilePath())
                                               .StripTrailingSeparators()
                                               .BaseName());

  std::string::size_type separator = config_name.find('-');
  if (separator != std::string::npos)
    config_name = config_name.substr(0, separator);

  DCHECK(!config_name.empty());
  return config_name;
}

// Returns the path to root_src_dir from settings.
std::string SourcePathFromBuildSettings(const BuildSettings* build_settings) {
  return RebasePath("//", build_settings->build_dir());
}

// Returns the default attributes for the project from settings.
PBXAttributes ProjectAttributesFromBuildSettings(
    const BuildSettings* build_settings) {
  const TargetOsType target_os = GetTargetOs(build_settings->build_args());

  PBXAttributes attributes;
  switch (target_os) {
    case WRITER_TARGET_OS_IOS:
      attributes["SDKROOT"] = "iphoneos";
      attributes["TARGETED_DEVICE_FAMILY"] = "1,2";
      break;
    case WRITER_TARGET_OS_MACOS:
      attributes["SDKROOT"] = "macosx";
      break;
  }

  return attributes;
}

}  // namespace

class XcodeProject {
 public:
  XcodeProject(const BuildSettings* build_settings,
               XcodeWriter::Options options,
               const std::string& name);
  ~XcodeProject();

  bool AddSourcesFromBuilder(const Builder& builder, Err* err);
  bool AddTargetsFromBuilder(const Builder& builder, Err* err);
  bool AssignIds(Err* err);

  bool WriteFile(Err* err) const;

 private:
  bool AddAggregateTarget(base::Environment* env, Err* err);
  PBXNativeTarget* AddBinaryTarget(const Target* target,
                                   base::Environment* env,
                                   Err* err);
  PBXNativeTarget* AddBundleTarget(const Target* target,
                                   base::Environment* env,
                                   Err* err);

  void WriteFileContent(std::ostream& out) const;

  std::optional<std::vector<const Target*>> GetTargetsFromBuilder(
      const Builder& builder,
      Err* err) const;

  const BuildSettings* build_settings_;
  XcodeWriter::Options options_;
  PBXProject project_;
};

XcodeProject::XcodeProject(const BuildSettings* build_settings,
                           XcodeWriter::Options options,
                           const std::string& name)
    : build_settings_(build_settings),
      options_(options),
      project_(name,
               ConfigNameFromBuildSettings(build_settings),
               SourcePathFromBuildSettings(build_settings),
               ProjectAttributesFromBuildSettings(build_settings)) {}

XcodeProject::~XcodeProject() = default;

bool XcodeProject::AddSourcesFromBuilder(const Builder& builder, Err* err) {
  SourceFileSet sources;

  // Add sources from all targets.
  for (const Target* target : builder.GetAllResolvedTargets()) {
    for (const SourceFile& source : target->sources()) {
      if (IsStringInOutputDir(build_settings_->build_dir(), source.value()))
        continue;

      sources.insert(source);
    }

    for (const SourceFile& source : target->public_headers()) {
      if (IsStringInOutputDir(build_settings_->build_dir(), source.value()))
        continue;

      sources.insert(source);
    }
  }

  const SourceDir source_dir("//");
  for (const SourceFile& source : sources) {
    const std::string source_file = RebasePath(
        source.value(), source_dir, build_settings_->root_path_utf8());
    project_.AddSourceFileToIndexingTarget(source_file, source_file,
                                           CompilerFlags::NONE);
  }

  return true;
}

bool XcodeProject::AddTargetsFromBuilder(const Builder& builder, Err* err) {
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  if (!AddAggregateTarget(env.get(), err))
    return false;

  const std::optional<std::vector<const Target*>> targets =
      GetTargetsFromBuilder(builder, err);
  if (!targets)
    return false;

  std::vector<const Target*> bundle_targets;
  TargetToPBXTarget bundle_target_to_pbxtarget;

  const SourceDir source_dir("//");

  // Needs to search for xctest files under the application targets, and this
  // variable is used to store the results of visited targets, thus making the
  // search more efficient.
  TargetToFileList xctest_files_per_target;

  const TargetOsType target_os = GetTargetOs(build_settings_->build_args());

  for (const Target* target : *targets) {
    PBXNativeTarget* native_target = nullptr;
    switch (target->output_type()) {
      case Target::EXECUTABLE:
        if (target_os == WRITER_TARGET_OS_IOS)
          continue;

        native_target = AddBinaryTarget(target, env.get(), err);
        if (!native_target)
          return false;

        break;

      case Target::CREATE_BUNDLE: {
        if (target->bundle_data().product_type().empty())
          continue;

        // For XCUITest, two CREATE_BUNDLE targets are generated:
        // ${target_name}_runner and ${target_name}_module, however, Xcode
        // requires only one target named ${target_name} to run tests.
        if (IsXCUITestRunnerTarget(target))
          continue;

        native_target = AddBundleTarget(target, env.get(), err);
        if (!native_target)
          return false;

        bundle_targets.push_back(target);
        bundle_target_to_pbxtarget.insert(
            std::make_pair(target, native_target));

        if (!IsXCTestModuleTarget(target) && !IsXCUITestModuleTarget(target))
          continue;

        // For XCTest, test files are compiled into the application bundle.
        // For XCUITest, test files are compiled into the test module bundle.
        const Target* target_with_xctest_files = nullptr;
        if (IsXCTestModuleTarget(target)) {
          target_with_xctest_files = FindApplicationTargetByName(
              target->defined_from(),
              target->bundle_data().xcode_test_application_name(), *targets,
              err);
          if (!target_with_xctest_files)
            return false;
        } else {
          DCHECK(IsXCUITestModuleTarget(target));
          target_with_xctest_files = target;
        }

        SearchXCTestFilesForTarget(target_with_xctest_files,
                                   &xctest_files_per_target);
        const Target::FileList& xctest_file_list =
            xctest_files_per_target[target_with_xctest_files];

        // Add xctest files to the "Compiler Sources" of corresponding xctest
        // and xcuitest native targets for proper indexing and for discovery of
        // tests function.
        AddXCTestFilesToTestModuleTarget(xctest_file_list, native_target,
                                         &project_, source_dir,
                                         build_settings_);

        break;
      }

      default:
        break;
    }
  }

  // Adding the corresponding test application target as a dependency of xctest
  // or xcuitest module target in the generated Xcode project so that the
  // application target is re-compiled when compiling the test module target.
  if (!AddDependencyTargetForTestModuleTargets(
          bundle_targets, bundle_target_to_pbxtarget, &project_, err)) {
    return false;
  }

  return true;
}

bool XcodeProject::AssignIds(Err* err) {
  RecursivelyAssignIds(&project_);
  return true;
}

bool XcodeProject::WriteFile(Err* err) const {
  DCHECK(!project_.id().empty());

  SourceFile pbxproj_file = build_settings_->build_dir().ResolveRelativeFile(
      Value(nullptr, project_.Name() + ".xcodeproj/project.pbxproj"), err);
  if (pbxproj_file.is_null())
    return false;

  std::stringstream pbxproj_string_out;
  WriteFileContent(pbxproj_string_out);

  return WriteFileIfChanged(build_settings_->GetFullPath(pbxproj_file),
                            pbxproj_string_out.str(), err);
}

bool XcodeProject::AddAggregateTarget(base::Environment* env, Err* err) {
  project_.AddAggregateTarget(
      "All",
      GetBuildScript(options_.root_target_name, options_.ninja_executable,
                     options_.ninja_extra_args, env));

  return true;
}

PBXNativeTarget* XcodeProject::AddBinaryTarget(const Target* target,
                                               base::Environment* env,
                                               Err* err) {
  DCHECK_EQ(target->output_type(), Target::EXECUTABLE);

  return project_.AddNativeTarget(
      target->label().name(), "compiled.mach-o.executable",
      target->output_name().empty() ? target->label().name()
                                    : target->output_name(),
      "com.apple.product-type.tool",
      GetBuildScript(target->label().name(), options_.ninja_executable,
                     options_.ninja_extra_args, env));
}

PBXNativeTarget* XcodeProject::AddBundleTarget(const Target* target,
                                               base::Environment* env,
                                               Err* err) {
  DCHECK_EQ(target->output_type(), Target::CREATE_BUNDLE);

  std::string pbxtarget_name = target->label().name();
  if (IsXCUITestModuleTarget(target)) {
    std::string target_name = target->label().name();
    pbxtarget_name = target_name.substr(
        0, target_name.rfind(kXCTestModuleTargetNamePostfix));
  }

  PBXAttributes xcode_extra_attributes =
      target->bundle_data().xcode_extra_attributes();

  const std::string& target_output_name = RebasePath(
      target->bundle_data().GetBundleRootDirOutput(target->settings()).value(),
      build_settings_->build_dir());
  return project_.AddNativeTarget(
      pbxtarget_name, std::string(), target_output_name,
      target->bundle_data().product_type(),
      GetBuildScript(pbxtarget_name, options_.ninja_executable,
                     options_.ninja_extra_args, env),
      xcode_extra_attributes);
}

void XcodeProject::WriteFileContent(std::ostream& out) const {
  out << "// !$*UTF8*$!\n"
      << "{\n"
      << "\tarchiveVersion = 1;\n"
      << "\tclasses = {\n"
      << "\t};\n"
      << "\tobjectVersion = 46;\n"
      << "\tobjects = {\n";

  for (auto& pair : CollectPBXObjectsPerClass(&project_)) {
    out << "\n"
        << "/* Begin " << ToString(pair.first) << " section */\n";
    std::sort(pair.second.begin(), pair.second.end(),
              [](const PBXObject* a, const PBXObject* b) {
                return a->id() < b->id();
              });
    for (auto* object : pair.second) {
      object->Print(out, 2);
    }
    out << "/* End " << ToString(pair.first) << " section */\n";
  }

  out << "\t};\n"
      << "\trootObject = " << project_.Reference() << ";\n"
      << "}\n";
}

std::optional<std::vector<const Target*>> XcodeProject::GetTargetsFromBuilder(
    const Builder& builder,
    Err* err) const {
  std::vector<const Target*> all_targets = builder.GetAllResolvedTargets();

  // Filter targets according to the dir_filters_string if defined.
  if (!options_.dir_filters_string.empty()) {
    std::vector<LabelPattern> filters;
    if (!commands::FilterPatternsFromString(
            build_settings_, options_.dir_filters_string, &filters, err)) {
      return std::nullopt;
    }

    std::vector<const Target*> unfiltered_targets;
    std::swap(unfiltered_targets, all_targets);

    commands::FilterTargetsByPatterns(unfiltered_targets, filters,
                                      &all_targets);
  }

  std::vector<const Target*> targets;
  targets.reserve(all_targets.size());

  // Filter out all target of type EXECUTABLE that are direct dependency of
  // a BUNDLE_DATA target (under the assumption that they will be part of a
  // CREATE_BUNDLE target generating an application bundle). Sort the list
  // of targets per pointer to use binary search for the removal.
  std::sort(all_targets.begin(), all_targets.end());

  for (const Target* target : all_targets) {
    if (!target->settings()->is_default())
      continue;

    if (target->output_type() != Target::BUNDLE_DATA)
      continue;

    for (const auto& pair : target->GetDeps(Target::DEPS_LINKED)) {
      if (pair.ptr->output_type() != Target::EXECUTABLE)
        continue;

      auto iter = std::lower_bound(targets.begin(), targets.end(), pair.ptr);
      if (iter != targets.end() && *iter == pair.ptr)
        targets.erase(iter);
    }
  }

  // Sort the list of targets per-label to get a consistent ordering of them
  // in the generated Xcode project (and thus stability of the file generated).
  std::sort(
      targets.begin(), targets.end(),
      [](const Target* a, const Target* b) { return a->label() < b->label(); });

  return targets;
}

class XcodeWorkspace {
 public:
  XcodeWorkspace(const BuildSettings* settings, XcodeWriter::Options options);
  ~XcodeWorkspace();

  std::unique_ptr<XcodeProject> CreateProject(const std::string& name);

  bool WriteFile(Err* err) const;

 private:
  void WriteFileContent(std::ostream& out) const;

  const BuildSettings* build_settings_;
  std::vector<std::string> project_names_;
  XcodeWriter::Options options_;
};

XcodeWorkspace::XcodeWorkspace(const BuildSettings* build_settings,
                               XcodeWriter::Options options)
    : build_settings_(build_settings), options_(options) {
  if (options_.workspace_name.empty())
    options.workspace_name.assign("all");
}

XcodeWorkspace::~XcodeWorkspace() = default;

std::unique_ptr<XcodeProject> XcodeWorkspace::CreateProject(
    const std::string& name) {
  DCHECK(!base::ContainsValue(project_names_, name));
  project_names_.push_back(name);
  return std::make_unique<XcodeProject>(build_settings_, options_, name);
}

bool XcodeWorkspace::WriteFile(Err* err) const {
  SourceFile xcworkspacedata_file =
      build_settings_->build_dir().ResolveRelativeFile(
          Value(nullptr, options_.workspace_name +
                             ".xcworkspace/contents.xcworkspacedata"),
          err);
  if (xcworkspacedata_file.is_null())
    return false;

  std::stringstream xcworkspacedata_string_out;
  WriteFileContent(xcworkspacedata_string_out);

  return WriteFileIfChanged(build_settings_->GetFullPath(xcworkspacedata_file),
                            xcworkspacedata_string_out.str(), err);
}

void XcodeWorkspace::WriteFileContent(std::ostream& out) const {
  out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
      << "<Workspace version = \"1.0\">\n";
  for (const std::string& project_name : project_names_) {
    out << "  <FileRef location = \"group:" << project_name
        << ".xcodeproj\"></FileRef>\n";
  }
  out << "</Workspace>\n";
}

// static
bool XcodeWriter::RunAndWriteFiles(const BuildSettings* build_settings,
                                   const Builder& builder,
                                   Options options,
                                   Err* err) {
  XcodeWriter writer(build_settings, options);
  if (!writer.AddSourcesFromBuilder(builder, err))
    return false;

  if (!writer.AddTargetsFromBuilder(builder, err))
    return false;

  if (!writer.WriteFiles(err))
    return false;

  return true;
}

XcodeWriter::XcodeWriter(const BuildSettings* build_settings, Options options) {
  workspace_ = std::make_unique<XcodeWorkspace>(build_settings, options);
  products_ = workspace_->CreateProject("products");
}

XcodeWriter::~XcodeWriter() = default;

bool XcodeWriter::AddSourcesFromBuilder(const Builder& builder, Err* err) {
  return products_->AddSourcesFromBuilder(builder, err);
}

bool XcodeWriter::AddTargetsFromBuilder(const Builder& builder, Err* err) {
  return products_->AddTargetsFromBuilder(builder, err);
}

bool XcodeWriter::WriteFiles(Err* err) const {
  if (!products_->AssignIds(err))
    return false;

  if (!products_->WriteFile(err))
    return false;

  if (!workspace_->WriteFile(err))
    return false;

  return true;
}
