#include <idalingo/translation.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <queue>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cn {

bool IsShortcutText(std::string_view source) noexcept {
    if (source.size() >= 5 && source.starts_with("sys(") && source.back() == ')') {
        return true;
    }

    constexpr std::array<std::string_view, 16> named_keys{"PgUp", "PgDown", "Up", "Down", "Left",
        "Right", "Home", "End", "Enter", "Return", "Esc", "Escape", "Tab", "Backtab", "Ins",
        "Delete"};
    if (std::find(named_keys.begin(), named_keys.end(), source) != named_keys.end()) {
        return true;
    }

    constexpr std::array<std::string_view, 10> modifier_prefixes{
        "Ctrl-", "Shift-", "Alt-", "Meta-", "Cmd-", "Ctrl+", "Shift+", "Alt+", "Meta+", "Cmd+"};
    return std::any_of(modifier_prefixes.begin(), modifier_prefixes.end(),
        [source](std::string_view prefix) { return source.starts_with(prefix); });
}

namespace {

bool AppendUtf8(std::string& output, std::uint32_t codepoint) {
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        return false;
    }
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return true;
}

bool DecodeUtf8(std::string_view text, std::size_t& offset, std::uint32_t& codepoint) {
    if (offset >= text.size()) {
        return false;
    }
    const auto lead = static_cast<unsigned char>(text[offset++]);
    if (lead <= 0x7F) {
        codepoint = lead;
        return true;
    }
    int continuation_count = 0;
    std::uint32_t minimum = 0;
    if ((lead & 0xE0) == 0xC0) {
        codepoint = lead & 0x1F;
        continuation_count = 1;
        minimum = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        codepoint = lead & 0x0F;
        continuation_count = 2;
        minimum = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        codepoint = lead & 0x07;
        continuation_count = 3;
        minimum = 0x10000;
    } else {
        return false;
    }
    for (int index = 0; index < continuation_count; ++index) {
        if (offset >= text.size()) {
            return false;
        }
        const auto continuation = static_cast<unsigned char>(text[offset++]);
        if ((continuation & 0xC0) != 0x80) {
            return false;
        }
        codepoint = (codepoint << 6) | (continuation & 0x3F);
    }
    return codepoint >= minimum && codepoint <= 0x10FFFF &&
        !(codepoint >= 0xD800 && codepoint <= 0xDFFF);
}

struct Rule8 {
    std::string source;
    std::string replacement;
};

struct Rule16 {
    std::wstring source;
    std::wstring replacement;
};

template <typename Char>
class MultiReplacer {
public:
    using String = std::basic_string<Char>;
    using View = std::basic_string_view<Char>;

    void Build(const std::vector<std::pair<String, String>>& rules) {
        nodes_.clear();
        patterns_.clear();
        replacements_.clear();
        nodes_.emplace_back();

        for (const auto& [source, replacement] : rules) {
            if (source.empty()) {
                continue;
            }
            const std::size_t id = patterns_.size();
            patterns_.push_back(source);
            replacements_.push_back(replacement);
            std::size_t state = 0;
            for (Char ch : source) {
                auto [it, inserted] = nodes_[state].next.try_emplace(ch, nodes_.size());
                if (inserted) {
                    nodes_.emplace_back();
                }
                state = it->second;
            }
            nodes_[state].outputs.push_back(id);
        }

        std::queue<std::size_t> pending;
        for (const auto& [_, child] : nodes_[0].next) {
            nodes_[child].failure = 0;
            pending.push(child);
        }

        while (!pending.empty()) {
            const std::size_t state = pending.front();
            pending.pop();
            for (const auto& [ch, child] : nodes_[state].next) {
                std::size_t fallback = nodes_[state].failure;
                while (fallback != 0 && !nodes_[fallback].next.contains(ch)) {
                    fallback = nodes_[fallback].failure;
                }
                if (const auto it = nodes_[fallback].next.find(ch);
                    it != nodes_[fallback].next.end()) {
                    fallback = it->second;
                }
                nodes_[child].failure = fallback;
                const auto& inherited = nodes_[fallback].outputs;
                nodes_[child].outputs.insert(
                    nodes_[child].outputs.end(), inherited.begin(), inherited.end());
                pending.push(child);
            }
        }
    }

    [[nodiscard]] String Replace(View text) const {
        if (text.empty() || patterns_.empty()) {
            return String(text);
        }

        constexpr std::size_t none = static_cast<std::size_t>(-1);
        std::vector<std::size_t> best(text.size(), none);
        std::size_t state = 0;

        for (std::size_t i = 0; i < text.size(); ++i) {
            const Char ch = text[i];
            while (state != 0 && !nodes_[state].next.contains(ch)) {
                state = nodes_[state].failure;
            }
            if (const auto it = nodes_[state].next.find(ch); it != nodes_[state].next.end()) {
                state = it->second;
            }
            for (std::size_t id : nodes_[state].outputs) {
                const std::size_t length = patterns_[id].size();
                if (length > i + 1) {
                    continue;
                }
                const std::size_t start = i + 1 - length;
                if (best[start] == none || length > patterns_[best[start]].size() ||
                    (length == patterns_[best[start]].size() && id > best[start])) {
                    best[start] = id;
                }
            }
        }

        String result;
        result.reserve(text.size());
        for (std::size_t i = 0; i < text.size();) {
            const std::size_t id = best[i];
            if (id != none) {
                result.append(replacements_[id]);
                i += patterns_[id].size();
            } else {
                result.push_back(text[i++]);
            }
        }
        return result;
    }

private:
    struct Node {
        std::unordered_map<Char, std::size_t> next;
        std::size_t failure = 0;
        std::vector<std::size_t> outputs;
    };

    std::vector<Node> nodes_{{}};
    std::vector<String> patterns_;
    std::vector<String> replacements_;
};

std::wstring Utf8ToWide(std::string_view text) {
    std::wstring result;
    result.reserve(text.size());
    std::size_t offset = 0;
    while (offset < text.size()) {
        std::uint32_t codepoint = 0;
        if (!DecodeUtf8(text, offset, codepoint)) {
            return {};
        }
        if constexpr (sizeof(wchar_t) == 2) {
            if (codepoint <= 0xFFFF) {
                result.push_back(static_cast<wchar_t>(codepoint));
            } else {
                codepoint -= 0x10000;
                result.push_back(static_cast<wchar_t>(0xD800 + (codepoint >> 10)));
                result.push_back(static_cast<wchar_t>(0xDC00 + (codepoint & 0x3FF)));
            }
        } else {
            result.push_back(static_cast<wchar_t>(codepoint));
        }
    }
    return result;
}

std::string DecodeUtf16Bytes(const std::vector<unsigned char>& bytes, bool big_endian) {
    std::string result;
    for (std::size_t index = 2; index + 1 < bytes.size(); index += 2) {
        std::uint32_t codepoint = big_endian
            ? static_cast<std::uint16_t>((bytes[index] << 8) | bytes[index + 1])
            : static_cast<std::uint16_t>(bytes[index] | (bytes[index + 1] << 8));
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
            if (index + 3 >= bytes.size()) {
                return {};
            }
            const std::uint32_t low = big_endian
                ? static_cast<std::uint16_t>((bytes[index + 2] << 8) | bytes[index + 3])
                : static_cast<std::uint16_t>(bytes[index + 2] | (bytes[index + 3] << 8));
            if (low < 0xDC00 || low > 0xDFFF) {
                return {};
            }
            codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
            index += 2;
        }
        if (!AppendUtf8(result, codepoint)) {
            return {};
        }
    }
    return result;
}

std::string DecodeFileBytes(std::vector<unsigned char> bytes) {
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        return std::string(reinterpret_cast<const char*>(bytes.data() + 3), bytes.size() - 3);
    }

    if (bytes.size() >= 2 &&
        ((bytes[0] == 0xFF && bytes[1] == 0xFE) || (bytes[0] == 0xFE && bytes[1] == 0xFF))) {
        const bool big_endian = bytes[0] == 0xFE;
        return DecodeUtf16Bytes(bytes, big_endian);
    }

    std::string raw(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return raw;
}

void TrimAscii(std::string_view& text) {
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
        text.remove_suffix(1);
    }
}

std::optional<std::string> ReadQuoted(std::string_view line, std::size_t& position) {
    while (position < line.size() && line[position] != '"') {
        ++position;
    }
    if (position == line.size()) {
        return std::nullopt;
    }
    ++position;

    std::string result;
    while (position < line.size()) {
        const char ch = line[position++];
        if (ch == '"') {
            return result;
        }
        if (ch == '\\' && position < line.size()) {
            const char escaped = line[position++];
            switch (escaped) {
            case '"':
                result.push_back('"');
                break;
            case '\\':
                result.push_back('\\');
                break;
            case 'n':
                result.push_back('\n');
                break;
            case 'r':
                result.push_back('\r');
                break;
            case 't':
                result.push_back('\t');
                break;
            default:
                result.push_back('\\');
                result.push_back(escaped);
                break;
            }
        } else {
            result.push_back(ch);
        }
    }
    return std::nullopt;
}

std::optional<Rule8> ParseRule(std::string_view line) {
    TrimAscii(line);
    if (line.empty() || line.front() == '#' || line.front() == ';') {
        return std::nullopt;
    }

    std::size_t position = 0;
    auto source = ReadQuoted(line, position);
    if (!source) {
        return std::nullopt;
    }
    const std::size_t comma = line.find(',', position);
    if (comma == std::string_view::npos) {
        return std::nullopt;
    }
    position = comma + 1;
    auto replacement = ReadQuoted(line, position);
    if (!replacement || source->empty()) {
        return std::nullopt;
    }
    return Rule8{std::move(*source), std::move(*replacement)};
}

std::vector<Rule8> BuiltInRules() {
    return {
        {"&File", "文件(&F)"},
        {"&Edit", "编辑(&E)"},
        {"&View", "视图(&V)"},
        {"View", "视图"},
        {"&Jump", "跳转(&J)"},
        {"&Help", "帮助(&H)"},
        {"Help", "帮助"},
        {"&About", "关于(&A)"},
        {"About", "关于"},
        {"Deb&ugger", "调试器(&U)"},
        {"Searc&h", "搜索(&H)"},
        {"&Options", "选项(&O)"},
        {"&Windows", "窗口(&W)"},
        {"Library function", "库函数"},
        {"Regular function", "常规函数"},
        {"External symbol", "外部符号"},
        {"Instruction", "指令"},
        {"Unexplored", "未分析"},
        {"Data", "数据"},
        {"Lumina function", "Lumina 函数"},
        {"IDA View", "IDA 视图"},
        {"Hex View", "HEX 视图"},
        {"Quick start", "快速启动"},
        {"IDA - The Interactive Disassembler", "IDA - 交互式反汇编软件"},
        {"Version", "版本"},
        {"64-bit address size", "64-位地址大小"},
        {"License:", "许可证:"},
        {"License expires on", "许可有效期至"},
        {"IDA Options", "IDA 选项"},
    };
}

} // namespace

struct TranslationTable::Impl {
    std::unordered_map<std::string, std::string> exact8;
    std::unordered_map<std::wstring, std::wstring> exact16;
    MultiReplacer<char> replacer8;
    MultiReplacer<wchar_t> replacer16;
    std::size_t rule_count = 0;

    void Build(const std::vector<Rule8>& rules) {
        exact8.clear();
        exact16.clear();
        std::vector<std::pair<std::string, std::string>> rules8;
        std::vector<std::pair<std::wstring, std::wstring>> rules16;
        rules8.reserve(rules.size());
        rules16.reserve(rules.size());

        for (const Rule8& rule : rules) {
            exact8.insert_or_assign(rule.source, rule.replacement);
            rules8.emplace_back(rule.source, rule.replacement);
            std::wstring source16 = Utf8ToWide(rule.source);
            std::wstring replacement16 = Utf8ToWide(rule.replacement);
            if (!source16.empty()) {
                exact16.insert_or_assign(source16, replacement16);
                rules16.emplace_back(std::move(source16), std::move(replacement16));
            }
        }

        replacer8.Build(rules8);
        replacer16.Build(rules16);
        rule_count = exact8.size();
    }
};

TranslationTable::TranslationTable() : impl_(std::make_unique<Impl>()) {
    impl_->Build(BuiltInRules());
}

TranslationTable::~TranslationTable() = default;

bool TranslationTable::LoadFile(const std::filesystem::path& path, std::wstring* status) {
    std::vector<Rule8> rules = BuiltInRules();
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        impl_->Build(rules);
        if (status) {
            *status = L"language.txt 未找到，已加载内置规则";
        }
        return false;
    }

    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    const std::string text = DecodeFileBytes(std::move(bytes));

    std::size_t loaded = 0;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = text.find('\n', start);
        std::string_view line(
            text.data() + start, (end == std::string::npos ? text.size() : end) - start);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        if (auto rule = ParseRule(line)) {
            rules.push_back(std::move(*rule));
            ++loaded;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }

    impl_->Build(rules);
    if (status) {
        *status = L"已加载 " + std::to_wstring(loaded) + L" 条外部翻译规则";
    }
    return true;
}

std::string TranslationTable::TranslateUtf8(std::string_view source, bool allow_substring) const {
    if (const auto it = impl_->exact8.find(std::string(source)); it != impl_->exact8.end()) {
        return it->second;
    }
    return allow_substring ? impl_->replacer8.Replace(source) : std::string(source);
}

std::wstring TranslationTable::TranslateUtf16(
    std::wstring_view source, bool allow_substring) const {
    if (const auto it = impl_->exact16.find(std::wstring(source)); it != impl_->exact16.end()) {
        return it->second;
    }
    return allow_substring ? impl_->replacer16.Replace(source) : std::wstring(source);
}

std::size_t TranslationTable::RuleCount() const noexcept {
    return impl_->rule_count;
}

} // namespace cn
