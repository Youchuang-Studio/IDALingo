#include "qt_hooks.hpp"
#include "winmm_proxy.hpp"

#include <Windows.h>

#include <cwchar>
#include <filesystem>
#include <iterator>

namespace {

bool IsTargetProcess() {
    wchar_t path[32768]{};
    if (GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path))) == 0) {
        return false;
    }
    const wchar_t* name = std::wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    return _wcsicmp(name, L"ida.exe") == 0 || _wcsicmp(name, L"ida64.exe") == 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void* reserved) {
    switch (reason) {
    case DLL_PROCESS_ATTACH: {
        DisableThreadLibraryCalls(instance);
        if (!cn::proxy::InitializeWinmmProxy()) {
            return FALSE;
        }
        if (IsTargetProcess()) {
            if (HANDLE worker = CreateThread(nullptr, 0, cn::StartQtHooks, instance, 0, nullptr)) {
                CloseHandle(worker);
            }
        }
        return TRUE;
    }
    case DLL_PROCESS_DETACH:
        if (reserved == nullptr) {
            cn::ShutdownQtHooks();
            cn::proxy::ShutdownWinmmProxy();
        }
        return TRUE;
    default:
        return TRUE;
    }
}
