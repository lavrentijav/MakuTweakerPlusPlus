#include "platform/Win32Window.h"

#include "app/Branding.h"
#include "ui/Dpi.h"

#include <cmath>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

namespace maku::platform {

static Win32Window* g_instance = nullptr;

static int ScalePx(const int logical, const float dpi) {
    return static_cast<int>(std::lround(static_cast<float>(logical) * dpi));
}

bool Win32Window::Create(HINSTANCE inst, const wchar_t* title) {
    g_instance = this;
    maku::ui::EnablePerMonitorDpi();

    const float dpi = maku::ui::GetDpiScale(nullptr);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = brand::kWindowClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(brand::kAppIconResourceId));
    wc.hIconSm = wc.hIcon;
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    const int lw = ScalePx(kWidth, dpi);
    const int lh = ScalePx(kHeight, dpi);
    RECT r{0, 0, lw, lh};
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    const int w = r.right - r.left;
    const int h = r.bottom - r.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    hwnd_ = CreateWindowExW(0, wc.lpszClassName,
                            title, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, x,
                            y, w, h, nullptr, nullptr, inst, nullptr);
    if (!hwnd_) return false;

    if (wc.hIcon) {
        SendMessageW(hwnd_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(wc.hIcon));
        SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(wc.hIconSm));
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
    return true;
}

bool Win32Window::ProcessMessages() {
    MSG msg{};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) return false;
    }
    return true;
}

void Win32Window::SetMessageHandler(std::function<void(UINT, WPARAM, LPARAM)> handler) {
    handler_ = std::move(handler);
}

void Win32Window::SetCloseInterceptor(std::function<bool()> handler) {
    closeInterceptor_ = std::move(handler);
}

LRESULT CALLBACK Win32Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;

    if (msg == WM_CLOSE && g_instance && g_instance->hwnd_ == hwnd &&
        g_instance->closeInterceptor_ && g_instance->closeInterceptor_())
        return 0;

    if (g_instance && g_instance->hwnd_ == hwnd && g_instance->handler_)
        g_instance->handler_(msg, wp, lp);

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace maku::platform
