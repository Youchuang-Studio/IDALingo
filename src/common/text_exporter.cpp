#include <idalingo/text_exporter.hpp>

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <type_traits>

namespace cn {
namespace {

std::string Escape(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result.push_back(ch);
            break;
        }
    }
    return result;
}

std::optional<std::string> ReadFirstField(std::string_view line) {
    std::size_t position = 0;
    while (
        position < line.size() && std::isspace(static_cast<unsigned char>(line[position])) != 0) {
        ++position;
    }
    if (position < line.size() && line[position] == 'L') {
        ++position;
    }
    if (position >= line.size() || line[position] != '"') {
        return std::nullopt;
    }
    ++position;

    std::string result;
    while (position < line.size()) {
        const char ch = line[position++];
        if (ch == '"') {
            return result;
        }
        if (ch != '\\' || position >= line.size()) {
            result.push_back(ch);
            continue;
        }
        switch (const char escaped = line[position++]) {
        case 'n':
            result.push_back('\n');
            break;
        case 'r':
            result.push_back('\r');
            break;
        case 't':
            result.push_back('\t');
            break;
        case '\\':
            result.push_back('\\');
            break;
        case '"':
            result.push_back('"');
            break;
        default:
            result.push_back('\\');
            result.push_back(escaped);
            break;
        }
    }
    return std::nullopt;
}

std::uint32_t CodeUnit(wchar_t unit) noexcept {
    return static_cast<std::make_unsigned_t<wchar_t>>(unit);
}

std::string WideToUtf8(std::wstring_view text) {
    std::string result;
    result.reserve(text.size());
    const auto append = [&result](std::uint32_t codepoint) {
        if (codepoint <= 0x7F) {
            result.push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else if (codepoint <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        }
    };
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t codepoint = CodeUnit(text[index]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (index + 1 >= text.size()) {
                    return {};
                }
                const std::uint32_t low = CodeUnit(text[++index]);
                if (low < 0xDC00 || low > 0xDFFF) {
                    return {};
                }
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
            } else if (codepoint >= 0xDC00 && codepoint <= 0xDFFF) {
                return {};
            }
        }
        if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
            return {};
        }
        append(codepoint);
    }
    return result;
}

bool IsUsefulText(std::string_view text) {
    if (text.empty() || text.size() > 10000 || text.find('\0') != std::string_view::npos) {
        return false;
    }
    for (const char raw : text) {
        const auto ch = static_cast<unsigned char>(raw);
        if (ch >= 0x80 || std::isalpha(ch) != 0) {
            return true;
        }
    }
    return false;
}

} // namespace

bool TextExporter::Open(const std::filesystem::path& path) {
    std::scoped_lock lock(mutex_);
    seen_.clear();
    if (output_.is_open()) {
        output_.close();
    }
    output_.clear();

    std::ifstream input(path, std::ios::binary);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (auto source = ReadFirstField(line); source && !source->empty()) {
            seen_.insert(std::move(*source));
        }
    }

    output_.open(path, std::ios::binary | std::ios::app);
    return output_.is_open();
}

void TextExporter::RecordUtf8(std::string_view text) {
    if (!IsUsefulText(text)) {
        return;
    }
    std::scoped_lock lock(mutex_);
    if (!output_.is_open() || !seen_.emplace(text).second) {
        return;
    }
    const std::string escaped = Escape(text);
    output_ << "L\"" << escaped << "\",L\"" << escaped << "\",\r\n";
    output_.flush();
}

void TextExporter::RecordUtf16(std::wstring_view text) {
    RecordUtf8(WideToUtf8(text));
}

} // namespace cn
