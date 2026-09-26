#pragma once

#include <string>
#include <vector>
#include <string_view>

namespace td {

struct MsdTag {
    std::string name;
    std::vector<std::string> params;

    [[nodiscard]] std::string value(size_t index = 0) const {
        if (index < params.size()) {
            return params[index];
        }
        return "";
    }
};

class MsdFile {
public:
    static constexpr size_t kMaxFileSize = 16 * 1024 * 1024; // 16 MB allocation cap
    static constexpr size_t kMaxTags = 10000;                // 10,000 tag cap
    static constexpr size_t kMaxParamsPerTag = 1000;         // Max 1,000 parameters per tag
    static constexpr size_t kMaxParamLength = 1024 * 1024;   // 1 MB cap per parameter

    MsdFile() = default;

    bool read_file(const std::string& filepath);
    bool read_string(std::string_view content);

    [[nodiscard]] const std::vector<MsdTag>& tags() const { return tags_; }
    [[nodiscard]] size_t size() const { return tags_.size(); }
    [[nodiscard]] bool empty() const { return tags_.empty(); }

    // Case-insensitive tag query
    [[nodiscard]] const MsdTag* find_tag(std::string_view name) const;
    [[nodiscard]] std::string get_tag_value(std::string_view name, size_t param_idx = 0, const std::string& default_val = "") const;

private:
    std::vector<MsdTag> tags_;
};

} // namespace td
