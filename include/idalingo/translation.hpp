#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

namespace cn {

[[nodiscard]] bool IsShortcutText(std::string_view source) noexcept;

class TranslationTable {
public:
    TranslationTable();
    ~TranslationTable();

    TranslationTable(const TranslationTable&) = delete;
    TranslationTable& operator=(const TranslationTable&) = delete;

    // Built-in rules are always loaded first. Entries in the file override them.
    bool LoadFile(const std::filesystem::path& path, std::wstring* status = nullptr);

    [[nodiscard]] std::string TranslateUtf8(
        std::string_view source, bool allow_substring = true) const;

    [[nodiscard]] std::wstring TranslateUtf16(
        std::wstring_view source, bool allow_substring = true) const;

    [[nodiscard]] std::size_t RuleCount() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace cn
