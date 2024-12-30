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
    FILEGROUP,
    CC_LIBRARY,
    CC_BINARY,
};

using BazelValue = std::variant<bool, std::string, Label, std::vector<std::string>, std::vector<SourceFile>, std::vector<Label>>;

struct BazelTarget {
    Label label;
    BazelRule rule;
    std::unordered_map<std::string, BazelValue> kwargs;
    std::optional<std::vector<LabelPattern>> visibility;
};

std::ostream& operator<<(std::ostream& o, const BazelTarget&);

class BazelWriter {
  public:
    BazelTarget insert(const ResolvedTargetData& resolved, const Target& target);
    void Write(const base::FilePath& out_dir) const;

  private:
    BazelTarget insert_locked(const ResolvedTargetData& resolved, const Target& target);
    std::vector<Label> insert_targets(const ResolvedTargetData& resolved, const base::span<const Target*>& targets);

    // Use unique pointers so that the returned BazelTarget references remain valid.
    std::unordered_map<Label, BazelTarget> targets_;
    mutable std::mutex mutex_;
};



#endif