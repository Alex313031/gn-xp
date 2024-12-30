#include "gn/bazel_writer.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <ranges>
#include <regex>
#include <set>

#include "filesystem_utils.h"
#include "gn/config.h"

namespace {
const auto kArActions = "@rules_cc//cc/toolchains/actions:ar_actions";
const auto kAssemblyActions = "@rules_cc//cc/toolchains/actions:assembly_actions";
const auto kCCompileActions = "@rules_cc//cc/toolchains/actions:c_compile";
const auto kCppCompileActions = "@rules_cc//cc/toolchains/actions:cpp_compile_actions";
const auto kCompileActions = "@rules_cc//cc/toolchains/actions:compile_actions";
const auto kObjcCompileActions = "@rules_cc//cc/toolchains/actions:objc_compile";
const auto kObjcppCompileActions = "@rules_cc//cc/toolchains/actions:objcpp_compile";
const auto kLinkActions = "@rules_cc//cc/toolchains/actions:link_actions";
const auto kDynamicLibraryLinkActions = "@rules_cc//cc/toolchains/actions:dynamic_library_link_actions";
const auto kStaticLibraryLinkActions = "@rules_cc//cc/toolchains/actions:ar_actions";
const auto kLinkExecutableActions = "@rules_cc//cc/toolchains/actions:link_executable_actions";
}

void BazelTarget::AddHeaderKwarg(const Target& target) {
    std::vector<LabelOrFile> hdrs;
    for (const auto& hdr : target.public_headers()) {
        hdrs.push_back(hdr);
    }
    kwargs["hdrs"] = hdrs;
}

void BazelTarget::AddSrcKwarg(const Target& target, bool seperate_headers) {
    std::vector<LabelOrFile> srcs;
    for (const SourceFile& src : target.sources()) {
        if (seperate_headers && src.GetType() == SourceFile::Type::SOURCE_H) {
            std::get<std::vector<LabelOrFile>>(kwargs["hdrs"]).push_back(src);
        } else {
            srcs.push_back(src);
        }
    }
    kwargs["srcs"] = srcs;
}

void BazelTarget::AddDataKwarg(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target) {
    const auto& data = resolved.GetDataDeps(&target);
    kwargs["data"] = writer.insert_targets(resolved, data);
}

std::vector<Label> BazelTarget::GetDeps(BazelWriter& writer,  const ResolvedTargetData& resolved, const Target& target) {
    const auto& resolved_deps = resolved.GetLinkedDeps(&target);
    return writer.insert_targets(resolved, resolved_deps);
}

void BazelTarget::ConfigureFeatures(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target) {
    std::vector<std::string> features;
    for (const auto& config : target.configs()) {
        writer.insert_config(config.label, *config.ptr);
        // Always resolve it as an absolute label.
        features.push_back(config.label.GetUserVisibleName(false));
    }
    kwargs["features"] = features;
}

void BazelTarget::ConfigureCc(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target, bool is_binary) {
    rule = is_binary ? BazelRule::CC_BINARY : BazelRule::CC_LIBRARY;
    AddHeaderKwarg(target);
    AddSrcKwarg(target, /* seperate_headers=*/ true);
    const ResolvedTargetDeps& deps = resolved.GetTargetDeps(&target);
    AddDataKwarg(writer, resolved, target);
    kwargs["deps"] = GetDeps(writer, resolved, target);
    const ConfigValues& config = target.config_values();
    kwargs["defines"] = config.defines();
    kwargs["linkopts"] = config.ldflags();
    kwargs["copts"] = config.cflags();
    kwargs["cxxopts"] = config.cflags_cc();
    kwargs["conlyopts"] = config.cflags_c();
    ConfigureFeatures(writer, resolved, target);
}

void BazelTarget::ConfigureFilegroup(const Target& target) {
    rule = BazelRule::FILEGROUP;
    AddSrcKwarg(target);
    target.assert_no_deps();
}

void BazelTarget::ConfigureGroup(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target) {
    const std::vector<Label>& deps = GetDeps(writer, resolved, target);
    if (deps.size() == 1) {
        rule = BazelRule::ALIAS;
        kwargs["actual"] = deps[0];
    } else {
        // The rule will be unknown.
        kwargs["deps"] = deps;
    }
}

void BazelTarget::Configure(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target) {
  if (const Toolchain* toolchain = target.toolchain(); toolchain != nullptr) {
    writer.insert_toolchain(*toolchain, resolved);
  }
  switch (target.output_type()) {
    case Target::BUNDLE_DATA:
        break;
    case Target::SOURCE_SET:
        ConfigureFilegroup(target);
        break;
    case Target::STATIC_LIBRARY:
        kwargs["linkstatic"] = true;
        // fallthrough
     case Target::SHARED_LIBRARY:
      ConfigureCc(writer, resolved, target, /* is_binary= */ false);
      break;
    case Target::EXECUTABLE:
      ConfigureCc(writer, resolved, target, /* is_binary= */ true);
      break;
    case Target::RUST_LIBRARY:
        rule = BazelRule::RUST_LIBRARY;
        break;
    case Target::RUST_PROC_MACRO:
        rule = BazelRule::RUST_PROC_MACRO;
        break;
    case Target::GROUP:
        ConfigureGroup(writer, resolved, target);
    default:
        break;
    }
}

const BazelTarget& BazelWriter::insert(const ResolvedTargetData& resolved, const Target& target) {
    // TODO: Consider a RW lock to improve performance.
    const std::lock_guard<std::mutex> lock(mutex_);
    return insert_locked(resolved, target);
}

std::vector<Label> BazelWriter::insert_targets(const ResolvedTargetData& resolved, const base::span<const Target*>& targets) {
    std::vector<Label> labels;
    for (const Target* target : targets) {
        insert_locked(resolved, *target);
        labels.push_back(target->label());
    }
    return labels;
}

BazelPackage& BazelWriter::package(const SourceDir& directory) {
    if (auto it = packages_.find(directory); it != packages_.end()) {
        return it->second;
    }
    auto [it, _] = packages_.emplace(directory, BazelPackage{.source_dir = directory});
    return it->second;
}

std::pair<std::reference_wrapper<BazelTarget>, bool> BazelWriter::insert_empty(const Label& label, std::optional<std::vector<LabelPattern>> visibility) {
    BazelPackage& package = this->package(label.dir());
    if (auto it = package.targets.find(label.name()); it != package.targets.end()) {
        return {*it->second, false};
    }

    auto [it, _] = package.targets.emplace(
        label.name(),
        std::make_unique<BazelTarget>(label, visibility)
    );
    return {*it->second, true};
}

const BazelTarget& BazelWriter::insert_config(const Label& label, const Config& config) {
    auto [target_ref, inserted] = insert_empty(label);
    if (!inserted) {
        return target_ref.get();
    }
    BazelTarget& bazel_target = target_ref.get();
    bazel_target.rule = BazelRule::CC_FEATURE;
    std::vector<std::string> implies;
    for (const auto& subcfg : config.configs()) {
        const auto& target = insert_config(subcfg.label, *subcfg.ptr);
        // Since it's a "feature", it needs to be a string, not a label, so it needs to be absolute.
        implies.push_back(target.label.GetUserVisibleName(false));
    }
    bazel_target.kwargs["implies"] = implies;
    const auto& values = config.own_values();
    // TODO: DO THIS PROPERLY
    bazel_target.kwargs["feature_name"] = label.GetUserVisibleName(false);
    std::vector<Label> cc_args;

    auto insert_args = [&] (const std::string& suffix, std::vector<std::string> actions, std::vector<std::string> args) {
        if (!args.empty()) {
            cc_args.push_back(insert_cc_args(label.WithSuffix(suffix), actions, args).label);
        }
    };

    insert_args("aropts", {kArActions}, values.arflags());
    insert_args("asmopts", {kAssemblyActions}, values.asmflags());
    insert_args("copts", {kCCompileActions, kCppCompileActions}, values.cflags());
    insert_args("conlyopts", {kCCompileActions}, values.cflags_c());
    insert_args("cxxopts", {kCppCompileActions}, values.cflags_cc());
    insert_args("objcopts", {kObjcCompileActions}, values.cflags_objc());
    insert_args("objcppopts", {kObjcppCompileActions}, values.cflags_objcc());
    insert_args("ldopts", {kLinkActions}, values.ldflags());
    std::vector<std::string> defines;
    for (const auto& define : values.defines()) {
        // Does this work with gcc / msvc?
        defines.push_back(std::format("-D{}", define));
    }
    insert_args("defines", {"all_compile_actions"}, defines);
    // TODO: framework_dirs, frameworks, eweak_frameworks, include_dirs, lib_dirs, rustflags, rustenv, swiftflags.
    bazel_target.kwargs["args"] = cc_args;
    return bazel_target;
}

const BazelTarget& BazelWriter::insert_cc_args(const Label& label, std::vector<std::string> actions, std::vector<std::string> args) {
    auto [target_ref, inserted] = insert_empty(label);
    if (!inserted) {
        return target_ref.get();
    }
    BazelTarget& bazel_target = target_ref.get();
    bazel_target.rule = CC_ARGS;
    bazel_target.kwargs["args"] = args;
    bazel_target.kwargs["actions"] = actions;
    return bazel_target;
}

const BazelTarget& BazelWriter::insert_toolchain(const Toolchain& toolchain, const ResolvedTargetData& resolved) {
    auto cc_toolchain_label = toolchain.label().WithSuffix("cc");
    auto [target_ref, inserted] = insert_empty(cc_toolchain_label);
    if (!inserted) {
        return target_ref.get();
    }
    BazelTarget& bazel_target = target_ref.get();
    bazel_target.rule = CC_TOOLCHAIN;

    auto [tools_ref, _] = insert_empty(cc_toolchain_label.WithSuffix("tools"));
    BazelTarget& tools_target = tools_ref.get();
    tools_target.rule = CC_TOOL_MAP;

    std::map<std::string, Label> tools;
    auto add_tool = [&] (const char* name, std::vector<std::string> actions) {
        const auto tool = toolchain.GetToolAsC(name);
        if (tool != nullptr) {
            auto [tool_ref, _] = insert_empty(cc_toolchain_label.WithSuffix(name));
            BazelTarget& tool_target = tool_ref.get();
            tool_target.rule = CC_TOOL;
            for (const auto& action : actions) {
                tools[action] = tool_target.label;
            }
        }
    };
    add_tool("cxx", {kCppCompileActions});
    add_tool("cc", {kCCompileActions});
    add_tool("alink", {kStaticLibraryLinkActions});
    add_tool("solink", {kDynamicLibraryLinkActions});
    add_tool("link", {kLinkExecutableActions});
    tools_target.kwargs["tools"] = tools;
    bazel_target.kwargs["tool_map"] = tools_target.label;
    return bazel_target;
}

const BazelTarget& BazelWriter::insert_locked(const ResolvedTargetData& resolved, const Target& target) {
    auto [target_ref, inserted] = insert_empty(target.label());
    if (!inserted) {
        return target_ref.get();
    }
    BazelTarget& bazel_target = target_ref.get();
    bazel_target.Configure(*this,  resolved, target);
    return bazel_target;
}

std::string ToString(const BazelRule& v) {
    switch (v) {
        case BazelRule::UNKNOWN:
            return "unknown";
        case BazelRule::FILEGROUP:
            return "filegroup";
        case BazelRule::CC_LIBRARY:
            return "cc_library";
        case BazelRule::CC_BINARY:
            return "cc_binary";
        case BazelRule::CC_ARGS:
              return  "cc_args";
        case BazelRule::CC_FEATURE:
              return  "cc_feature";
        case BazelRule::CC_TOOLCHAIN:
              return  "cc_toolchain";
        case BazelRule::CC_TOOL:
              return  "cc_tool";
        case BazelRule::CC_TOOL_MAP:
              return  "cc_tool_map";
        case BazelRule::ALIAS:
            return "alias";
        case BazelRule::RUST_BINARY:
            return "rust_binary";
        case BazelRule::RUST_LIBRARY:
            return "rust_library";
        case BazelRule::RUST_PROC_MACRO:
            return "rust_proc_macro";
    }
}

std::string BzlFileDefining(const BazelRule& v) {
    switch (v) {
            case BazelRule::CC_BINARY:
            case BazelRule::CC_LIBRARY:
              return  "@rules_cc//cc:defs.bzl";
            case BazelRule::CC_FEATURE:
              return  "@rules_cc//cc/toolchains:feature.bzl";
            case BazelRule::CC_ARGS:
              return  "@rules_cc//cc/toolchains:args.bzl";
            case BazelRule::CC_TOOLCHAIN:
              return  "@rules_cc//cc/toolchains:toolchain.bzl";
            case BazelRule::CC_TOOL:
              return  "@rules_cc//cc/toolchains:tool.bzl";
            case BazelRule::CC_TOOL_MAP:
              return  "@rules_cc//cc/toolchains:tool_map.bzl";
            case BazelRule::ALIAS:
            case BazelRule::FILEGROUP:
                return "";
            case BazelRule::RUST_BINARY:
            case BazelRule::RUST_LIBRARY:
            case BazelRule::RUST_PROC_MACRO:
            return "@rules_rust//rust:defs.bzl";
            case BazelRule::UNKNOWN:
            return "//:unknown.bzl";
    }
}

std::ostream& Quote(std::ostream& o, const std::string& s) {
    o << '"' << std::regex_replace(s, std::regex("\""), "\\\"") << '"';
    return o;
}

std::string BazelTarget::ToString() const {
    std::stringstream o;
    o << ::ToString(rule) << "(\n    name = \"" << label.name() << "\",\n";
    SourceDir package = label.dir();
    for (const auto& [key, variant]: kwargs) {
        std::stringstream ss;
        if (const auto *val = std::get_if<bool>(&variant); val != nullptr) {
            ss << (*val ? "True" : "False");
        } else if (const auto *val = std::get_if<std::string>(&variant); val != nullptr) {
            Quote(ss, *val);
        } else if (const auto *val = std::get_if<Label>(&variant); val != nullptr) {
            ss << '"' << val->RelativeLabel(label.dir()) << '"';
        } else if (const auto *val = std::get_if<std::vector<std::string>>(&variant); val != nullptr && !val->empty()) {
            ss << "[\n";
            for (const auto& item : *val) {
                Quote(ss << "        ", item) << ",\n";
            }
            ss << "    ]";
        } else if (const auto *val = std::get_if<std::vector<Label>>(&variant); val != nullptr && !val->empty()) {
            ss << "[\n";
            for (const auto& label : *val) {
                Quote(ss << "        ", label.RelativeLabel(package)) << ",\n";
            }
            ss << "    ]";
        } else if (const auto *val = std::get_if<std::vector<LabelOrFile>>(&variant); val != nullptr && !val->empty()) {
            ss << "[\n";
            for (const auto& entry : *val) {
                if (const auto *label = std::get_if<Label>(&entry); label != nullptr) {
                    Quote(ss << "        ", label->RelativeLabel(package)) << ",\n";
                } else if (const auto *path = std::get_if<SourceFile>(&entry); path != nullptr) {
                    Quote(ss << "        ", std::filesystem::relative(path->value(), package.value()).generic_string())  << ",\n";
                }
            }
            ss << "    ]";
        } else if (const auto *val = std::get_if<std::map<std::string, Label>>(&variant); val != nullptr && !val->empty()) {
            ss << "{\n";
            for (const auto& [key, value] : *val) {
                Quote(Quote(ss << "        ", key) << ": ", label.RelativeLabel(package)) << ",\n";
            }
            ss << "    }";
        }

        if (!ss.view().empty()) {
            o << "    " << key << " = " << ss.view() << ",\n";
        }
    }
    // TODO: visibility
    o << ")\n";
    return o.str();
}

std::string BazelPackage::ToString() const {
    std::stringstream content;
    content << "# TODO: licensing\n";

    std::map<std::string, std::set<std::string>> loads;
    for (const auto& [name, target] : targets) {
        loads[BzlFileDefining(target->rule)].insert(::ToString(target->rule));
    }

    for (const auto& [file, rules] : loads) {
        if (file.empty()) {
            continue;
        }
        content << "load(\"" << file << "\"";
        for (const auto& rule : rules) {
            content << ", \"" << rule << "\"";
        }
        content << ")\n";
    }

    auto target_names_view = std::ranges::views::keys(targets);
    std::vector<std::string> target_names(target_names_view.begin(), target_names_view.end());
    std::ranges::sort(target_names);

    for (const auto& name : target_names) {
        content << "\n" << targets.find(name)->second->ToString();
    }

    if (!exported_files.empty()) {
        content << "\nexports_files([\n";
        for (const auto& file : exported_files) {
            content << "    \"" << file << "\",\n";
        }
        content << "])\n";
    }
    return content.str();
}

void BazelWriter::PostProcess() {
    // BUILD.bazel can't refer to files in subdirectories if those subdirectories have BUILD.bazel files.
    std::cout << "Adding exports_files where required." << std::endl;
    for (auto& package : std::ranges::views::values(packages_)) {
        for (auto& target : std::ranges::views::values(package.targets)) {
            for (auto& value : std::ranges::views::values(target->kwargs)) {
                if (auto* vec = std::get_if<std::vector<LabelOrFile>>(&value); vec != nullptr) {
                    for (auto& label : *vec) {
                        if (auto* file = std::get_if<SourceFile>(&label); file != nullptr) {
                            for (auto dir = file->GetDir(); dir != package.source_dir && !dir.is_null(); dir = dir.Parent()) {
                                if (auto it = packages_.find(dir); it != packages_.end()) {
                                    // Transform it from foo/subdir/bar.h to foo/subdir:bar.h
                                    std::string relative = std::filesystem::relative(file->value(), dir.value());
                                    it->second.exported_files.insert(relative);
                                    label = Label(dir, relative);
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    std::cout << "Done" << std::endl;

}

void BazelWriter::Write(const base::FilePath& out_dir) const {
    std::cout << "Writing build files to " << out_dir.MaybeAsASCII() << std::endl;
    for (const auto& [_, package]: packages_) {

        const auto build_file = package.source_dir.Resolve(out_dir).Append("BUILD.bazel");
        Err err;
        // TODO: nice error
        CHECK(WriteFile(build_file, package.ToString(), &err));
        // std::cout << "Wrote to " << build_file.MaybeAsASCII() << std::endl;
    }
}