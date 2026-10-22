#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

namespace cn {

class TextExporter {
public:
    bool Open(const std::filesystem::path& path);
    void RecordUtf8(std::string_view text);
    void RecordUtf16(std::wstring_view text);

private:
    std::mutex mutex_;
    std::unordered_set<std::string> seen_;
    std::ofstream output_;
};

} // namespace cn
