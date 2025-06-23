#include "gn/strict_deps.h"

#include "base/json/json_writer.h"
#include "base/values.h"
#include "gn/desc_builder.h"
#include "gn/file_writer.h"
#include "gn/output_file.h"
#include "gn/settings.h"
#include "gn/source_file.h"
#include "gn/target.h"

#include <vector>

constexpr int kSchemaVersion = 1;

using JsonValue = base::Value;

class JsonBuilder {
public:
    JsonBuilder(const BuildSettings* build_settings, const Builder& builder): build_settings_{build_settings}, builder_{builder} {}

    std::vector<JsonValue> CompressedStrings() {
        std::vector<JsonValue> out(strings_.size());
        for (auto& [k, v] : std::move(strings_)) {
            out[v] = JsonValue(k);
        }
        return out;
    }

    JsonValue Compress(std::string s) {
        auto [it, _] = strings_.try_emplace(std::move(s), strings_.size());
        return JsonValue(it->second);
    }

    std::vector<JsonValue> Compress(std::vector<std::string> v) {
        std::sort(v.begin(), v.end());
        std::vector<JsonValue> out;
        out.reserve(v.size());
        for (const auto& s : v) {
            out.push_back(Compress(s));
        }
        return out;
    }

    std::vector<JsonValue> Targets(const LabelTargetVector& targets, const Label& default_toolchain_label) {
        std::vector<std::string> out;
        out.reserve(targets.size());
        for (const auto& target : targets) {
            out.push_back(target.label.GetUserVisibleName(default_toolchain_label));
        }
        return Compress(out);
    }

    std::vector<JsonValue> Files(std::vector<SourceFile> files) {
        std::vector<std::string> out;
        out.reserve(files.size());
        for (const auto& f : files) {
            out.push_back(OutputFile(build_settings_, f).value());
        }
        return Compress(out);
    }

    JsonValue StrictDepsTargets() {
        std::vector<JsonValue> out;

        auto targets = builder_.GetAllResolvedTargets();
        std::sort(targets.begin(), targets.end(), [](const Target* lhs, const Target* rhs) {
            return lhs->label() < rhs->label();
        });
        if (targets.empty()) {
            return JsonValue(std::move(out));
        }
        const auto& default_toolchain = targets.front()->settings()->default_toolchain_label();
        const auto& build_root = build_settings_->build_dir();

        out.reserve(targets.size());
        for (const auto& target : targets) {
            base::flat_map<std::string, std::unique_ptr<JsonValue>> cur;
            cur["name"] = std::make_unique<JsonValue>(Compress(target->label().GetUserVisibleName(default_toolchain)));
            cur["deps"] = std::make_unique<JsonValue>(Targets(target->private_deps(), default_toolchain));
            cur["public_deps"] = std::make_unique<JsonValue>(Targets(target->public_deps(), default_toolchain));
            cur["public_headers"] = std::make_unique<JsonValue>(Files(target->public_headers()));
            cur["sources"] = std::make_unique<JsonValue>(Files(target->sources()));

            auto sources = target->sources();
            std::sort(sources.begin(), sources.end());
            base::flat_map<std::string, std::unique_ptr<JsonValue>> source_outputs;
            for (const auto& source : sources) {
                std::vector<OutputFile> output_files;
                auto tool = Tool::kToolNone;
                if (target->GetOutputFilesForSource(source, &tool, &output_files)) {
                    if (!output_files.empty()) {
                        std::sort(output_files.begin(), output_files.end());
                        std::vector<JsonValue> outputs;
                        outputs.reserve(output_files.size());
                        auto source_number = Compress(OutputFile(build_settings_, source).value()).GetInt();
                        for (const auto& output_file : output_files)
                            outputs.emplace_back(Compress(output_file.value()));
                        // JsonWriter doesn't handle int keys for dictionaries, and I don't feel like going to the effort of supporting it.
                        source_outputs[std::to_string(source_number)] = std::make_unique<JsonValue>(std::move(outputs));
                    }
                }

            }
            if (!source_outputs.empty()) {
                cur["source_outputs"] = std::make_unique<JsonValue>(std::move(source_outputs));
            }
            out.emplace_back(std::move(cur));
        }
        return JsonValue(std::move(out));
    }



private:
    const BuildSettings* build_settings_;
    const Builder& builder_;
    std::unordered_map<std::string, int> strings_;
};

bool WriteStrictDeps(const BuildSettings* settings, const Builder& builder, Err* err) {
    SourceFile output_file = settings->build_dir().ResolveRelativeFile(
        Value(nullptr, "strict-deps.json"), err);

    if (output_file.is_null()) {
        *err = Err(Value(), "Unable to resolve strict-deps.json");
        return false;
    }
    base::FilePath output_path = settings->GetFullPath(output_file);
    
    auto json_builder = JsonBuilder(settings, builder);
    base::flat_map<std::string, std::unique_ptr<JsonValue>> values;
    values["version"] = std::make_unique<JsonValue>(kSchemaVersion);
    values["targets"] = std::make_unique<JsonValue>(json_builder.StrictDepsTargets());
    values["strings"] = std::make_unique<JsonValue>(json_builder.CompressedStrings());

    std::string json;
    base::JSONWriter::Write(JsonValue(std::move(values)), &json);

    FileWriter writer;
    writer.Create(output_path);
    writer.Write(json);

    if (!writer.Close()) {
        *err = Err(Value(), "Unable to write strict-deps.json");
        return false;
    }
    return true;
}