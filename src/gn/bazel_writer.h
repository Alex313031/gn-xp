#ifndef TOOLS_GN_BAZEL_WRITER_H_
#define TOOLS_GN_BAZEL_WRITER_H_

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "base/files/file_path.h"
#include "gn/resolved_target_data.h"
#include "gn/source_file.h"
#include "gn/target.h"

enum BazelRule {
    UNKNOWN,
    ALIAS,
    FILEGROUP,
    CC_LIBRARY,
    CC_ARGS,
    CC_BINARY,
    CC_FEATURE,
    CC_TOOLCHAIN,
    CC_TOOL,
    CC_TOOL_MAP,
    RUST_LIBRARY,
    RUST_PROC_MACRO,
    RUST_BINARY,
};

std::string ToString(const BazelRule&);

using LabelOrFile = std::variant<Label, SourceFile>;
using BazelValue = std::variant<bool, std::string, Label, std::vector<std::string>, std::vector<Label>, std::vector<LabelOrFile>, std::map<std::string, Label>>;

class BazelWriter;

struct BazelTarget {
    // TODO: private + friend with BazelWriter/package?
    Label label;
    BazelRule rule;
    std::map<std::string, BazelValue> kwargs;
    std::optional<std::vector<LabelPattern>> visibility;

    BazelTarget(
        const Label& label,
        std::optional<std::vector<LabelPattern>> visibility = {}
    ):
       label{label}, rule{BazelRule::UNKNOWN}, visibility{visibility} {}


    std::string ToString() const;
    void Configure(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target);

  private:
    void AddHeaderKwarg(const Target& target);
    void AddSrcKwarg(const Target& target, bool seperate_headers=false);
    void AddDataKwarg(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target);
    std::vector<Label> GetDeps(BazelWriter& writer,  const ResolvedTargetData& resolved, const Target& target);
    void ConfigureCc(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target, bool is_binary);
    void ConfigureFilegroup(const Target& target);
    void ConfigureGroup(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target);
    void ConfigureFeatures(BazelWriter& writer, const ResolvedTargetData& resolved, const Target& target);
};

// TODO: fine-grained RW-locks once multithreading introduced.
struct BazelPackage {
  // TODO: private + friend with BazelWriter?
    SourceDir source_dir;
    std::map<std::string, std::unique_ptr<BazelTarget>> targets;
    std::set<std::string> exported_files;

    std::string ToString() const;
};

// TODO: Rename to BazelGraph?
class BazelWriter {
  public:
    BazelPackage& package(const SourceDir& directory);
    const BazelTarget& insert(const ResolvedTargetData& resolved, const Target& target);
    void PostProcess();
    void Write(const base::FilePath& out_dir) const;

  private:
    std::pair<std::reference_wrapper<BazelTarget>, bool> insert_empty(const Label& label, std::optional<std::vector<LabelPattern>> visibility = {});
    const BazelTarget& insert_locked(const ResolvedTargetData& resolved, const Target& target);
    std::vector<Label> insert_targets(const ResolvedTargetData& resolved, const base::span<const Target*>& targets);
    const BazelTarget& insert_config(const Label& label, const Config& config);
    const BazelTarget& insert_cc_args(const Label& label, std::vector<std::string> actions, std::vector<std::string> args);
    const BazelTarget& insert_toolchain(const Toolchain& toolchain, const ResolvedTargetData& resolved);

    std::unordered_map<SourceDir, BazelPackage> packages_;
    mutable std::mutex mutex_;

    friend struct BazelTarget;
};



#endif