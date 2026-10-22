#include "winmm_proxy.hpp"

#include "winmm_exports.hpp"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <string>

extern "C" std::uintptr_t g_winmm_exports[cn::proxy::kExportCount]{};

namespace cn::proxy {
namespace {

HMODULE g_real_winmm = nullptr;

extern "C" std::uintptr_t __fastcall MissingWinmmExport() {
    SetLastError(ERROR_PROC_NOT_FOUND);
    return 0;
}

void LogMissing(const char* name, std::size_t ordinal) {
    std::string line = "[IDALingo] winmm export missing: ";
    line += name ? name : "ordinal";
    line += " #" + std::to_string(ordinal) + "\n";
    OutputDebugStringA(line.c_str());
}

} // namespace

bool InitializeWinmmProxy() {
    wchar_t system_directory[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(system_directory, MAX_PATH);
    if (length == 0 || length >= MAX_PATH - 11) {
        return false;
    }

    std::wstring path(system_directory, length);
    path += L"\\winmm.dll";
    g_real_winmm = LoadLibraryW(path.c_str());
    if (!g_real_winmm) {
        return false;
    }

    for (std::size_t index = 0; index < kExportCount; ++index) {
        const std::size_t ordinal = index + 2;
        const char* name = kExportNames[index];
        const auto identifier =
            name ? name : reinterpret_cast<const char*>(static_cast<std::uintptr_t>(ordinal));
        FARPROC address = GetProcAddress(g_real_winmm, identifier);
        if (!address) {
            LogMissing(name, ordinal);
            address = reinterpret_cast<FARPROC>(&MissingWinmmExport);
        }
        g_winmm_exports[index] = reinterpret_cast<std::uintptr_t>(address);
    }
    return true;
}

void ShutdownWinmmProxy() {
    if (g_real_winmm) {
        FreeLibrary(g_real_winmm);
        g_real_winmm = nullptr;
    }
}

} // namespace cn::proxy
