#pragma once
#include <functional>
#include <windows.h>

namespace maku::platform {

class Win32Window {
public:
    // Wider than the WPF original's 1062: the Fluent rows carry a 40px switch
    // and a larger font, and at the old width long labels ran under the edge.
    static constexpr int kWidth = 1240;
    static constexpr int kHeight = 760;

    bool Create(HINSTANCE inst, const wchar_t* title);
    HWND Hwnd() const { return hwnd_; }
    bool ProcessMessages();
    void SetMessageHandler(std::function<void(UINT, WPARAM, LPARAM)> handler);
    /// Return true from WM_CLOSE to hide instead of destroying the window.
    void SetCloseInterceptor(std::function<bool()> handler);

private:
    HWND hwnd_{};
    std::function<void(UINT, WPARAM, LPARAM)> handler_;
    std::function<bool()> closeInterceptor_;
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

} // namespace maku::platform
