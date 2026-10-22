#pragma once

#include <cstddef>
#include <string>

namespace cn {

bool ProbeQString(const void* object, const wchar_t** characters, std::size_t* length) noexcept;
std::wstring ReadQString(const void* object);

} // namespace cn
