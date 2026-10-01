#include "chart/msd_file.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cctype>

namespace blaze4k {

namespace {

void trim_in_place(std::string& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' || s.front() == '\n')) {
        s.erase(s.begin());
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
}

bool case_insensitive_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

bool MsdFile::read_file(const std::string& filepath) {
    tags_.clear();

    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "[MsdFile] Could not open file: " << filepath << "\n";
        return false;
    }

    auto file_size = file.tellg();
    if (file_size < 0) {
        std::cerr << "[MsdFile] Invalid file size for: " << filepath << "\n";
        return false;
    }
    if (static_cast<size_t>(file_size) > kMaxFileSize) {
        std::cerr << "[MsdFile] File exceeds maximum size of 16MB: " << filepath << " ("
                  << file_size << " bytes)\n";
        return false;
    }

    file.seekg(0, std::ios::beg);
    std::string buffer(static_cast<size_t>(file_size), '\0');
    if (!file.read(&buffer[0], file_size)) {
        std::cerr << "[MsdFile] Failed reading file: " << filepath << "\n";
        return false;
    }

    return read_string(buffer);
}

bool MsdFile::read_string(std::string_view content) {
    tags_.clear();

    if (content.size() > kMaxFileSize) {
        std::cerr << "[MsdFile] Input content exceeds 16MB limit.\n";
        return false;
    }

    size_t i = 0;
    const size_t len = content.size();

    while (i < len && tags_.size() < kMaxTags) {
        // Skip whitespace and comments outside of tags
        if (content[i] == '/' && i + 1 < len && content[i + 1] == '/') {
            i += 2;
            while (i < len && content[i] != '\n' && content[i] != '\r') {
                i++;
            }
            continue;
        }

        if (content[i] != '#') {
            i++;
            continue;
        }

        // Found start of tag '#'
        i++; // skip '#'

        MsdTag tag;
        std::string current_param;
        bool in_tag = true;

        while (i < len && in_tag) {
            // Check for comment inside tag
            if (content[i] == '/' && i + 1 < len && content[i + 1] == '/') {
                i += 2;
                while (i < len && content[i] != '\n' && content[i] != '\r') {
                    i++;
                }
                while (i < len && (content[i] == '\n' || content[i] == '\r')) {
                    i++;
                }
                continue;
            }

            char c = content[i];
            if (c == '\\' && i + 1 < len) {
                // Escape sequence
                current_param += content[i + 1];
                i += 2;
                continue;
            }

            if (c == '#') {
                // Encountering '#' inside an unclosed tag ends the current tag (SM5 behavior)
                trim_in_place(current_param);
                if (tag.name.empty()) {
                    tag.name = current_param;
                } else if (tag.params.size() < kMaxParamsPerTag) {
                    tag.params.push_back(current_param);
                }
                current_param.clear();
                in_tag = false;
                break; // Do not advance i; outer loop will process this '#'
            }

            if (c == ':') {
                trim_in_place(current_param);
                if (tag.name.empty()) {
                    tag.name = current_param;
                } else if (tag.params.size() < kMaxParamsPerTag) {
                    tag.params.push_back(current_param);
                }
                current_param.clear();
                i++;
            } else if (c == ';') {
                trim_in_place(current_param);
                if (tag.name.empty()) {
                    tag.name = current_param;
                } else if (tag.params.size() < kMaxParamsPerTag) {
                    tag.params.push_back(current_param);
                }
                current_param.clear();
                in_tag = false;
                i++;
            } else {
                if (current_param.size() < kMaxParamLength) {
                    current_param += c;
                }
                i++;
            }
        }

        // If tag wasn't closed by ';' before EOF, still accept what was read
        if (in_tag && !current_param.empty()) {
            trim_in_place(current_param);
            if (tag.name.empty()) {
                tag.name = current_param;
            } else if (tag.params.size() < kMaxParamsPerTag) {
                tag.params.push_back(current_param);
            }
        }

        if (!tag.name.empty()) {
            tags_.push_back(std::move(tag));
        }
    }

    return true;
}

const MsdTag* MsdFile::find_tag(std::string_view name) const {
    for (const auto& tag : tags_) {
        if (case_insensitive_equal(tag.name, name)) {
            return &tag;
        }
    }
    return nullptr;
}

std::string MsdFile::get_tag_value(std::string_view name, size_t param_idx, const std::string& default_val) const {
    const MsdTag* tag = find_tag(name);
    if (!tag) {
        return default_val;
    }
    return tag->value(param_idx);
}

} // namespace blaze4k
