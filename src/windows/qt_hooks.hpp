#pragma once

#include <Windows.h>

namespace cn {

DWORD WINAPI StartQtHooks(void* module);
void ShutdownQtHooks();

} // namespace cn
