#include "gn/bazel_writer.h"

#include <filesystem>
#include <iostream>
#include <map>
#include <set>

#include "filesystem_utils.h"

BazelTarget BazelWriter::insert(const ResolvedTargetData& resolved, const Target& target) {
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

BazelTarget BazelWriter::insert_locked(const ResolvedTargetData& resolved, const Target& target) {
    if (auto it = targets_.find(target.label()); it != targets_.end()) {
        return it->second;
    }

    bool has_srcs = true;
    bool has_deps = true;
    bool has_data = true;
    bool has_hdrs = false;
    auto rule = BazelRule::UNKNOWN;
    std::unordered_map<std::string, BazelValue> kwargs;
    if (target.output_type() == Target::BUNDLE_DATA) {
        rule = BazelRule::FILEGROUP;
        has_data = false;
        has_hdrs = false;
        has_deps = false;
        target.assert_no_deps();
    } else if (target.output_type() == Target::SHARED_LIBRARY ||
               target.output_type() == Target::STATIC_LIBRARY) {
      rule = BazelRule::CC_LIBRARY;
      has_hdrs = true;
      if (target.output_type() == Target::STATIC_LIBRARY) {
        kwargs["linkstatic"] = true;
      }
    } else if (target.output_type() == Target::EXECUTABLE) {
      rule = BazelRule::CC_BINARY;
      has_hdrs = true;
    } else {
      CHECK(0) << "Output type of target not handled.";
    }

    std::vector<SourceFile> hdrs;

    if (has_hdrs) {
        hdrs = target.public_headers();
    }
    if (has_srcs) {
        std::vector<SourceFile> srcs;
        for (const SourceFile& src : target.sources()) {
            if (src.GetName().ends_with(".h")) {
                hdrs.push_back(src);
            } else {
                srcs.push_back(src);
            }
        }
        kwargs["srcs"] = srcs;
    }
    const ResolvedTargetDeps& deps = resolved.GetTargetDeps(&target);
    if (has_data) {
        kwargs["data"] = insert_targets(resolved, deps.data_deps());
    }

    if (has_deps) {
        kwargs["deps"] = insert_targets(resolved, deps.linked_deps());
    }

    kwargs["hdrs"] = hdrs;

    auto output = BazelTarget{
        .label = target.label(),
        .rule = rule,
        .kwargs = kwargs,
        .visibility = target.friends(),
    };

    targets_[target.label()] = output; 
    return output;
}

std::string rule_name(const BazelRule& v) {
    switch (v) {
        case BazelRule::UNKNOWN:
            return "unknown";
        case BazelRule::FILEGROUP:
            return "filegroup";
        case BazelRule::CC_LIBRARY:
            return "cc_library";
        case BazelRule::CC_BINARY:
            return "cc_binary";
    }
}

std::ostream& operator<<(std::ostream& o, const BazelRule& v) {
    return o << rule_name(v);
}

std::string rule_bzl_file(const BazelRule& v) {
    switch (v) {
            case BazelRule::CC_BINARY:
            case BazelRule::CC_LIBRARY:
              return  "@rules_cc//cc:defs.bzl";
            case BazelRule::FILEGROUP:
            case BazelRule::UNKNOWN:
            return "";
    }
}

std::ostream& operator<<(std::ostream& o, const BazelTarget& v) {
    o << v.rule << "(\n    name = \"" << v.label.name() << "\",\n";
    SourceDir package = v.label.dir();
    for (const auto& [key, variant]: v.kwargs) {
        std::stringstream ss;
        if (const auto *val = std::get_if<bool>(&variant); val != nullptr) {
            ss << (*val ? "True" : "False");
        } else if (const auto *val = std::get_if<std::string>(&variant); val != nullptr) {
            ss << '"' << *val << '"';
        } else if (const auto *val = std::get_if<Label>(&variant); val != nullptr) {
            ss << '"' << val->RelativeLabel(v.label.dir()) << '"';
        } else if (const auto *val = std::get_if<std::vector<std::string>>(&variant); val != nullptr && !val->empty()) {
            ss << "[\n";
            for (const auto& item : *val) {
                ss << "        \"" << item << "\",\n";
            }
            ss << "    ]";
        } else if (const auto *val = std::get_if<std::vector<SourceFile>>(&variant); val != nullptr && !val->empty()) {
            ss << "[\n";
            for (const auto& path : *val) {
                ss << "        \"" << std::filesystem::relative(path.value(), package.value()).generic_string()  << "\",\n";
            }
            ss << "    ]";
        } else if (const auto *val = std::get_if<std::vector<Label>>(&variant); val != nullptr && !val->empty()) {
            ss << "[\n";
            for (const auto& label : *val) {
                ss << "        \"" << label.RelativeLabel(v.label.dir()) << "\",\n";
            }
            ss << "    ]";
        }
        if (!ss.view().empty()) {
            o << "    " << key << " = " << ss.view() << ",\n";
        }
    }
    // TODO: visibility
    o << ")\n";
    return o;
}

void BazelWriter::Write(const base::FilePath& out_dir) const {
    std::cout << "Writing build files to " << out_dir.MaybeAsASCII() << std::endl;
    auto cmp = [] (const std::reference_wrapper<const BazelTarget>& lhs, const std::reference_wrapper<const BazelTarget>& rhs) {
        return lhs.get().label.name() < rhs.get().label.name();
    };
    std::unordered_map<base::FilePath, std::vector<std::reference_wrapper<const BazelTarget>>> packages;
    for (const auto& [label, target] : targets_) {
        packages[label.dir().Resolve(out_dir)].push_back(target);
    }

    for (auto& [package, targets]: packages) {
        std::stringstream content;
        content << "# TODO: licensing\n";

        std::map<std::string, std::set<std::string>> loads;
        for (const auto& target : targets) {
            loads[rule_bzl_file(target.get().rule)].insert(rule_name(target.get().rule));
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

        std::sort(targets.begin(), targets.end(), cmp);

        for (const auto& target : targets) {
            content << "\n" << target.get();
        }

        const auto build_file = package.Append("BUILD.bazel");
        Err err;
        // TODO: nice error
        CHECK(WriteFile(build_file, content.str(), &err));
        std::cout << "Wrote to " << build_file.MaybeAsASCII() << std::endl;
    }
}