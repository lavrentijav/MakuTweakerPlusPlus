#pragma once
#include <windows.h>
#include <shellapi.h>
#include <string>

namespace maku::platform {

class TrayIcon {
public:
    bool Create(HWND hwnd, HINSTANCE inst);
    void Destroy();
    void ShowBalloon(const std::wstring& title, const std::wstring& message);
    bool HandleMessage(UINT msg, WPARAM wp, LPARAM lp);
    bool ProcessCallback(HWND hwnd, WPARAM wp, LPARAM lp);

private:
    HWND hwnd_{};
    NOTIFYICONDATAW nid_{};
};

} // namespace maku::platform
