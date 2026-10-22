#include "qt_string_abi.hpp"

#include <Windows.h>

#include <cstdint>

namespace cn {

bool ProbeQString(const void* object, const wchar_t** characters, std::size_t* length) noexcept {
    if (!object || !characters || !length) {
        return false;
    }
#if defined(_MSC_VER)
    __try {
        const auto* fields = reinterpret_cast<const std::uintptr_t*>(object);
        const auto* text = reinterpret_cast<const wchar_t*>(fields[1]);
        const auto count = static_cast<std::int64_t>(fields[2]);
        if (count < 0 || count > 10000) {
            return false;
        }
        if (count == 0) {
            *characters = L"";
            *length = 0;
            return true;
        }
        if (!text) {
            return false;
        }
        volatile wchar_t probe = text[count - 1];
        (void)probe;
        *characters = text;
        *length = static_cast<std::size_t>(count);
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    return false;
#endif
}

std::wstring ReadQString(const void* object) {
    const wchar_t* characters = nullptr;
    std::size_t length = 0;
    if (!ProbeQString(object, &characters, &length)) {
        return {};
    }
    return std::wstring(characters, length);
}

} // namespace cn
