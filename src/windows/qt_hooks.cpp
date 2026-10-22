#include "qt_hooks.hpp"

#include "qt_string_abi.hpp"
#include <idalingo/text_exporter.hpp>
#include <idalingo/translation.hpp>

#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace cn {
namespace {

using QStringFromUtf8Fn = void*(__fastcall*)(void*, const char*, std::int64_t);
using QCoreTranslateFn = void*(__fastcall*)(void*, const char*, const char*, const char*, int);
using QStringCtorFn = void*(__fastcall*)(void*, const wchar_t*, std::int64_t);
using QStringDtorFn = void(__fastcall*)(void*);
using SetQStringFn = void(__fastcall*)(void*, const void*);
using DrawTextFn = void(__fastcall*)(void*, const void*, unsigned int, const void*, void*);

QStringFromUtf8Fn g_from_utf8 = nullptr;
QCoreTranslateFn g_translate = nullptr;
QStringCtorFn g_qstring_ctor = nullptr;
QStringDtorFn g_qstring_dtor = nullptr;
SetQStringFn g_set_title = nullptr;
SetQStringFn g_set_text = nullptr;
DrawTextFn g_draw_text = nullptr;

TranslationTable g_table;
#if defined(IDALINGO_EXPORT_ONLY)
TextExporter g_exporter;
#endif
std::atomic_bool g_installed{false};

thread_local bool g_inside_from_utf8 = false;
thread_local bool g_inside_translate = false;
thread_local bool g_inside_title = false;
thread_local bool g_inside_label = false;
thread_local bool g_inside_draw = false;

class RecursionGuard {
public:
    explicit RecursionGuard(bool& flag) : flag_(flag) { flag_ = true; }
    ~RecursionGuard() { flag_ = false; }
    RecursionGuard(const RecursionGuard&) = delete;
    RecursionGuard& operator=(const RecursionGuard&) = delete;

private:
    bool& flag_;
};

void Log(std::wstring_view message) {
    std::wstring line = L"[IDALingo] ";
    line.append(message);
    line.push_back(L'\n');
    OutputDebugStringW(line.c_str());
}

class TemporaryQString {
public:
    explicit TemporaryQString(std::wstring_view text) {
        if (g_qstring_ctor) {
            g_qstring_ctor(storage_.data(), text.data(), static_cast<std::int64_t>(text.size()));
            constructed_ = true;
        }
    }

    ~TemporaryQString() {
        if (constructed_ && g_qstring_dtor) {
            g_qstring_dtor(storage_.data());
        }
    }

    TemporaryQString(const TemporaryQString&) = delete;
    TemporaryQString& operator=(const TemporaryQString&) = delete;

    [[nodiscard]] const void* get() const noexcept { return storage_.data(); }
    [[nodiscard]] bool valid() const noexcept { return constructed_; }

private:
    alignas(16) std::array<std::byte, 24> storage_{};
    bool constructed_ = false;
};

void* __fastcall HookFromUtf8(void* result, const char* source, std::int64_t length) {
    if (!source || g_inside_from_utf8) {
        return g_from_utf8(result, source, length);
    }
    RecursionGuard guard(g_inside_from_utf8);
    const std::size_t actual = length < 0 ? std::strlen(source) : static_cast<std::size_t>(length);
    if (actual > std::size_t{1024} * 1024) {
        return g_from_utf8(result, source, length);
    }
    const std::string_view original(source, actual);
#if defined(IDALINGO_EXPORT_ONLY)
    g_exporter.RecordUtf8(original);
    return g_from_utf8(result, source, length);
#else
    if (IsShortcutText(original)) {
        return g_from_utf8(result, source, length);
    }
    const std::string translated = g_table.TranslateUtf8(original, false);
    if (translated == original) {
        return g_from_utf8(result, source, length);
    }
    return g_from_utf8(result, translated.data(), static_cast<std::int64_t>(translated.size()));
#endif
}

void* __fastcall HookTranslate(
    void* result, const char* context, const char* source, const char* disambiguation, int plural) {
    if (!source || g_inside_translate) {
        return g_translate(result, context, source, disambiguation, plural);
    }
    RecursionGuard guard(g_inside_translate);
    const std::string_view original(source);
#if defined(IDALINGO_EXPORT_ONLY)
    g_exporter.RecordUtf8(original);
    return g_translate(result, context, source, disambiguation, plural);
#else
    const std::string translated = g_table.TranslateUtf8(original, false);
    return g_translate(result, context, translated == original ? source : translated.c_str(),
        disambiguation, plural);
#endif
}

template <typename Callback>
void TranslateQStringArgument(const void* source, bool& recursion_flag, const Callback& callback) {
    if (!source || recursion_flag) {
        callback(source);
        return;
    }
    RecursionGuard guard(recursion_flag);
    const std::wstring original = ReadQString(source);
    if (original.empty()) {
        callback(source);
        return;
    }
#if defined(IDALINGO_EXPORT_ONLY)
    g_exporter.RecordUtf16(original);
    callback(source);
#else
    const std::wstring translated = g_table.TranslateUtf16(original, true);
    if (translated == original) {
        callback(source);
        return;
    }
    TemporaryQString temporary(translated);
    callback(temporary.valid() ? temporary.get() : source);
#endif
}

void __fastcall HookSetTitle(void* self, const void* text) {
    TranslateQStringArgument(
        text, g_inside_title, [self](const void* replacement) { g_set_title(self, replacement); });
}

void __fastcall HookSetText(void* self, const void* text) {
    TranslateQStringArgument(
        text, g_inside_label, [self](const void* replacement) { g_set_text(self, replacement); });
}

void __fastcall HookDrawText(
    void* self, const void* rect, unsigned int flags, const void* text, void* bounding_rect) {
    TranslateQStringArgument(text, g_inside_draw, [=](const void* replacement) {
        g_draw_text(self, rect, flags, replacement, bounding_rect);
    });
}

FARPROC Resolve(HMODULE module, const char* name) {
    return module ? GetProcAddress(module, name) : nullptr;
}

bool Install(HMODULE core, HMODULE gui, HMODULE widgets) {
    auto* from_utf8 = Resolve(core, "?fromUtf8@QString@QT@@SA?AV12@PEBD_J@Z");
    auto* translate = Resolve(core, "?translate@QCoreApplication@QT@@SA?AVQString@2@PEBD00H@Z");
    auto* set_title = Resolve(gui, "?setTitle@QWindow@QT@@QEAAXAEBVQString@2@@Z");
    auto* set_text = Resolve(widgets, "?setText@QLabel@QT@@QEAAXAEBVQString@2@@Z");
    auto* draw_text =
        Resolve(gui, "?drawText@QPainter@QT@@QEAAXAEBVQRect@2@HAEBVQString@2@PEAV32@@Z");
    g_qstring_ctor =
        reinterpret_cast<QStringCtorFn>(Resolve(core, "??0QString@QT@@QEAA@PEBVQChar@1@_J@Z"));
    g_qstring_dtor = reinterpret_cast<QStringDtorFn>(Resolve(core, "??1QString@QT@@QEAA@XZ"));

    if (!from_utf8 || !translate || !set_title || !set_text || !draw_text || !g_qstring_ctor ||
        !g_qstring_dtor) {
        Log(L"Qt 6 ABI 符号不匹配，钩子未安装");
        return false;
    }

    const MH_STATUS initialize = MH_Initialize();
    if (initialize != MH_OK && initialize != MH_ERROR_ALREADY_INITIALIZED) {
        Log(L"MinHook 初始化失败");
        return false;
    }

    struct Hook {
        void* target;
        void* detour;
        void** original;
    };
    const std::array hooks{
        Hook{reinterpret_cast<void*>(from_utf8), reinterpret_cast<void*>(&HookFromUtf8),
            reinterpret_cast<void**>(&g_from_utf8)},
        Hook{reinterpret_cast<void*>(translate), reinterpret_cast<void*>(&HookTranslate),
            reinterpret_cast<void**>(&g_translate)},
        Hook{reinterpret_cast<void*>(set_title), reinterpret_cast<void*>(&HookSetTitle),
            reinterpret_cast<void**>(&g_set_title)},
        Hook{reinterpret_cast<void*>(set_text), reinterpret_cast<void*>(&HookSetText),
            reinterpret_cast<void**>(&g_set_text)},
        Hook{reinterpret_cast<void*>(draw_text), reinterpret_cast<void*>(&HookDrawText),
            reinterpret_cast<void**>(&g_draw_text)},
    };

    std::vector<void*> created;
    for (const Hook& hook : hooks) {
        if (MH_CreateHook(hook.target, hook.detour, hook.original) != MH_OK) {
            for (void* target : created) {
                MH_RemoveHook(target);
            }
            MH_Uninitialize();
            Log(L"创建 Qt 钩子失败");
            return false;
        }
        created.push_back(hook.target);
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
        for (void* target : created) {
            MH_RemoveHook(target);
        }
        MH_Uninitialize();
        Log(L"启用 Qt 钩子失败");
        return false;
    }

    g_installed = true;
    return true;
}

} // namespace

DWORD WINAPI StartQtHooks(void* module) {
    wchar_t module_path[32768]{};
    const DWORD length = GetModuleFileNameW(
        static_cast<HMODULE>(module), module_path, static_cast<DWORD>(std::size(module_path)));
    const std::filesystem::path module_directory = length > 0
        ? std::filesystem::path(module_path).parent_path()
        : std::filesystem::current_path();
#if defined(IDALINGO_EXPORT_ONLY)
    std::filesystem::path export_path;
    bool export_open = false;
    if (!export_open) {
        export_path = module_directory / L"language_export.txt";
        export_open = g_exporter.Open(export_path);
    }
    if (!export_open) {
        wchar_t local_app_data[32768]{};
        const DWORD local_length = GetEnvironmentVariableW(
            L"LOCALAPPDATA", local_app_data, static_cast<DWORD>(std::size(local_app_data)));
        if (local_length == 0 || local_length >= std::size(local_app_data)) {
            Log(L"文本导出路径初始化失败");
            return 3;
        }
        const std::filesystem::path export_directory =
            std::filesystem::path(local_app_data) / L"IDALingo";
        std::error_code error;
        std::filesystem::create_directories(export_directory, error);
        export_path = export_directory / L"language_export.txt";
        if (error || !g_exporter.Open(export_path)) {
            Log(L"文本导出文件打开失败");
            return 3;
        }
    }
    Log(L"文本导出到 " + export_path.wstring());
#else
    std::wstring status;
    g_table.LoadFile(module_directory / L"language.txt", &status);
    Log(status + L"；有效规则 " + std::to_wstring(g_table.RuleCount()) + L" 条");
#endif

    HMODULE core = nullptr;
    HMODULE gui = nullptr;
    HMODULE widgets = nullptr;
    for (int attempt = 0; attempt < 1200; ++attempt) {
        core = GetModuleHandleW(L"Qt6Core.dll");
        gui = GetModuleHandleW(L"Qt6Gui.dll");
        widgets = GetModuleHandleW(L"Qt6Widgets.dll");
        if (core && gui && widgets) {
            break;
        }
        Sleep(100);
    }

    if (!core || !gui || !widgets) {
        Log(L"等待 Qt 6 模块超时");
        return 1;
    }
    if (!Install(core, gui, widgets)) {
        return 2;
    }
#if defined(IDALINGO_EXPORT_ONLY)
    Log(L"Qt 6 文本采集钩子安装完成");
#else
    Log(L"Qt 6 汉化钩子安装完成");
#endif
    return 0;
}

void ShutdownQtHooks() {
    if (!g_installed.exchange(false)) {
        return;
    }
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
}

} // namespace cn
