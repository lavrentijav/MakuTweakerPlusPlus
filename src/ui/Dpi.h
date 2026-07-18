#pragma once
#include <windows.h>

namespace maku::ui {

/// Windows display scale (1.0 = 96 DPI).
float GetDpiScale(HWND hwnd = nullptr);

void EnablePerMonitorDpi();

} // namespace maku::ui
