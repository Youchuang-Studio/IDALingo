#include <idalingo/text_exporter.hpp>
#include <idalingo/translation.hpp>

#include <dlfcn.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace {

struct ByteArrayView {
    std::ptrdiff_t size;
    const char* data;
};

struct QtString {
    void* header;
    const char16_t* characters;
    std::ptrdiff_t size;
};

struct QtArrayData {
    std::atomic<std::int32_t> reference_count;
    std::uint32_t flags;
    std::ptrdiff_t allocation;
};

static_assert(sizeof(ByteArrayView) == 16);
static_assert(sizeof(QtString) == 24);
static_assert(sizeof(QtArrayData) == 16);

constexpr const char* kFromUtf8 = "_ZN2QT7QString8fromUtf8ENS_14QByteArrayViewE";
constexpr const char* kTranslate = "_ZN2QT16QCoreApplication9translateEPKcS2_S2_i";
constexpr const char* kSetText = "_ZN2QT6QLabel7setTextERKNS_7QStringE";
constexpr const char* kSetWindowTitle = "_ZN2QT7QWidget14setWindowTitleERKNS_7QStringE";
constexpr const char* kDrawText = "_ZN2QT8QPainter8drawTextERKNS_5QRectEiRKNS_7QStringEPS1_";
constexpr const char* kDeallocate = "_ZN2QT10QArrayData10deallocateEPS0_xx";

using FromUtf8Function = QtString (*)(ByteArrayView);
using TranslateFunction = QtString (*)(const char*, const char*, const char*, int);
using SetStringFunction = void (*)(void*, const QtString*);
using DrawTextFunction = void (*)(void*, const void*, int, const QtString*, void*);
using DeallocateFunction = void (*)(void*, std::ptrdiff_t, std::ptrdiff_t);

template <typename Function>
Function Resolve(const char* name) {
    void* symbol = dlvsym(RTLD_NEXT, name, "Qt_6");
    if (!symbol) {
        symbol = dlsym(RTLD_NEXT, name);
    }
    if (!symbol) {
        std::fprintf(stderr, "[IDALingo] unresolved Qt symbol: %s\n", name);
        std::abort();
    }
    return reinterpret_cast<Function>(symbol);
}

FromUtf8Function OriginalFromUtf8() {
    static const auto function = Resolve<FromUtf8Function>(kFromUtf8);
    return function;
}

TranslateFunction OriginalTranslate() {
    static const auto function = Resolve<TranslateFunction>(kTranslate);
    return function;
}

SetStringFunction OriginalSetText() {
    static const auto function = Resolve<SetStringFunction>(kSetText);
    return function;
}

SetStringFunction OriginalSetWindowTitle() {
    static const auto function = Resolve<SetStringFunction>(kSetWindowTitle);
    return function;
}

DrawTextFunction OriginalDrawText() {
    static const auto function = Resolve<DrawTextFunction>(kDrawText);
    return function;
}

DeallocateFunction OriginalDeallocate() {
    static const auto function = Resolve<DeallocateFunction>(kDeallocate);
    return function;
}

void AppendUtf8(std::string& output, std::uint32_t codepoint) {
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
}

std::string QStringToUtf8(const QtString& text) {
    if (!text.characters || text.size <= 0 || text.size > std::ptrdiff_t{64} * 1024 * 1024) {
        return {};
    }
    std::string result;
    result.reserve(static_cast<std::size_t>(text.size));
    for (std::ptrdiff_t index = 0; index < text.size; ++index) {
        std::uint32_t codepoint = text.characters[index];
        if (codepoint >= 0xD800 && codepoint <= 0xDBFF && index + 1 < text.size) {
            const std::uint32_t low = text.characters[index + 1];
            if (low >= 0xDC00 && low <= 0xDFFF) {
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (low - 0xDC00);
                ++index;
            }
        }
        if (codepoint >= 0xD800 && codepoint <= 0xDFFF) {
            codepoint = 0xFFFD;
        }
        AppendUtf8(result, codepoint);
    }
    return result;
}

std::filesystem::path ModuleDirectory() {
    Dl_info information{};
    if (dladdr(reinterpret_cast<void*>(&ModuleDirectory), &information) == 0 ||
        !information.dli_fname) {
        return std::filesystem::current_path();
    }
    return std::filesystem::weakly_canonical(information.dli_fname).parent_path();
}

std::once_flag g_initialize_once;
std::unique_ptr<cn::TranslationTable> g_translations;
std::unique_ptr<cn::TextExporter> g_exporter;
thread_local bool g_inside_hook = false;

class RecursionGuard {
public:
    RecursionGuard() { g_inside_hook = true; }
    ~RecursionGuard() { g_inside_hook = false; }
    RecursionGuard(const RecursionGuard&) = delete;
    RecursionGuard& operator=(const RecursionGuard&) = delete;
};

void Initialize() {
    std::call_once(g_initialize_once, [] {
        const std::filesystem::path module_directory = ModuleDirectory();
#if defined(IDALINGO_EXPORT_ONLY)
        g_exporter = std::make_unique<cn::TextExporter>();
        const std::filesystem::path export_path = module_directory / "language_export.txt";
        if (!g_exporter->Open(export_path)) {
            std::fprintf(stderr, "[IDALingo] failed to open %s\n", export_path.c_str());
            g_exporter.reset();
        }
#else
        std::filesystem::path language_path = module_directory / "language.txt";
        if (const char* override_path = std::getenv("IDALINGO_LANGUAGE");
            override_path && *override_path) {
            language_path = override_path;
        }
        g_translations = std::make_unique<cn::TranslationTable>();
        std::wstring status;
        g_translations->LoadFile(language_path, &status);
        std::fprintf(stderr, "[IDALingo] loaded %zu rules from %s\n",
            g_translations->RuleCount(), language_path.c_str());
#endif
    });
}

std::string ProcessText(std::string_view source, bool allow_substring) {
    Initialize();
#if defined(IDALINGO_EXPORT_ONLY)
    (void)allow_substring;
    if (g_exporter) {
        g_exporter->RecordUtf8(source);
    }
    return std::string(source);
#else
    if (!g_translations || cn::IsShortcutText(source)) {
        return std::string(source);
    }
    return g_translations->TranslateUtf8(source, allow_substring);
#endif
}

void ReleaseQString(QtString& text) noexcept {
    auto* header = static_cast<QtArrayData*>(text.header);
    if (!header) {
        return;
    }
    const std::int32_t count = header->reference_count.load(std::memory_order_relaxed);
    if (count <= 0) {
        return;
    }
    if (header->reference_count.fetch_sub(1, std::memory_order_acq_rel) == 1) {
        OriginalDeallocate()(header, sizeof(char16_t), alignof(char16_t));
    }
    text = {};
}

template <typename Callback>
void WithTranslatedQString(const QtString* source, const Callback& callback) {
    if (!source || g_inside_hook) {
        callback(source);
        return;
    }
    RecursionGuard guard;
    try {
        const std::string original = QStringToUtf8(*source);
        if (original.empty() && source->size != 0) {
            callback(source);
            return;
        }
        const std::string translated = ProcessText(original, true);
        if (translated == original) {
            callback(source);
            return;
        }
        QtString replacement = OriginalFromUtf8()(
            ByteArrayView{static_cast<std::ptrdiff_t>(translated.size()), translated.data()});
        callback(&replacement);
        ReleaseQString(replacement);
    } catch (...) {
        callback(source);
    }
}

} // namespace

extern "C" __attribute__((visibility("default"))) QtString HookQStringFromUtf8(
    ByteArrayView view) asm("_ZN2QT7QString8fromUtf8ENS_14QByteArrayViewE");

extern "C" QtString HookQStringFromUtf8(ByteArrayView view) {
    const FromUtf8Function original_function = OriginalFromUtf8();
    if (!view.data || view.size < 0 || g_inside_hook) {
        return original_function(view);
    }
    RecursionGuard guard;
    try {
        const std::string source(view.data, static_cast<std::size_t>(view.size));
        const std::string translated = ProcessText(source, false);
        if (translated == source) {
            return original_function(view);
        }
        return original_function(
            ByteArrayView{static_cast<std::ptrdiff_t>(translated.size()), translated.data()});
    } catch (...) {
        return original_function(view);
    }
}

extern "C" __attribute__((visibility("default"))) QtString HookQCoreTranslate(const char* context,
    const char* source, const char* disambiguation,
    int plural) asm("_ZN2QT16QCoreApplication9translateEPKcS2_S2_i");

extern "C" QtString HookQCoreTranslate(
    const char* context, const char* source, const char* disambiguation, int plural) {
    const TranslateFunction original_function = OriginalTranslate();
    if (!source || g_inside_hook) {
        return original_function(context, source, disambiguation, plural);
    }
    RecursionGuard guard;
    try {
        const std::string translated = ProcessText(source, false);
        return original_function(
            context, translated == source ? source : translated.c_str(), disambiguation, plural);
    } catch (...) {
        return original_function(context, source, disambiguation, plural);
    }
}

extern "C" __attribute__((visibility("default"))) void HookQLabelSetText(
    void* self, const QtString& text) asm("_ZN2QT6QLabel7setTextERKNS_7QStringE");

extern "C" void HookQLabelSetText(void* self, const QtString& text) {
    WithTranslatedQString(
        &text, [self](const QtString* replacement) { OriginalSetText()(self, replacement); });
}

extern "C" __attribute__((visibility("default"))) void HookQWidgetSetWindowTitle(
    void* self, const QtString& text) asm("_ZN2QT7QWidget14setWindowTitleERKNS_7QStringE");

extern "C" void HookQWidgetSetWindowTitle(void* self, const QtString& text) {
    WithTranslatedQString(&text,
        [self](const QtString* replacement) { OriginalSetWindowTitle()(self, replacement); });
}

extern "C" __attribute__((visibility("default"))) void HookQPainterDrawText(void* self,
    const void* rectangle, int flags, const QtString& text,
    void* bounding_rectangle) asm("_ZN2QT8QPainter8drawTextERKNS_5QRectEiRKNS_7QStringEPS1_");

extern "C" void HookQPainterDrawText(
    void* self, const void* rectangle, int flags, const QtString& text, void* bounding_rectangle) {
    WithTranslatedQString(&text, [=](const QtString* replacement) {
        OriginalDrawText()(self, rectangle, flags, replacement, bounding_rectangle);
    });
}
