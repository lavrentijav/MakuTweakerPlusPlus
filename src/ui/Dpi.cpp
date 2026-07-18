#include "ui/Dpi.h"
#include <imgui_impl_win32.h>

namespace maku::ui {

void EnablePerMonitorDpi() {
    typedef BOOL(WINAPI * SetCtxFn)(DPI_AWARENESS_CONTEXT);
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        if (auto fn = reinterpret_cast<SetCtxFn>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
            fn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        }
    }
}

float GetDpiScale(HWND hwnd) {
    if (hwnd) {
        const float s = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);
        if (s > 0.5f) return s;
    }
    HDC hdc = GetDC(nullptr);
    if (!hdc) return 1.f;
    const int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);
    return dpi > 0 ? static_cast<float>(dpi) / 96.f : 1.f;
}

} // namespace maku::ui
