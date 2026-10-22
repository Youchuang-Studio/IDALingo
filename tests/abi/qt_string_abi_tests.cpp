#include "qt_string_abi.hpp"

#include <array>
#include <cstdint>

int main() {
    const std::wstring text = L"ABCDEF";
    const std::array<std::uintptr_t, 3> qstring{
        0x1234, reinterpret_cast<std::uintptr_t>(text.data()), text.size()};
    if (cn::ReadQString(qstring.data()) != text) {
        return 1;
    }

    const std::array<std::uintptr_t, 3> empty{0, 0, 0};
    if (!cn::ReadQString(empty.data()).empty()) {
        return 2;
    }
    return 0;
}
