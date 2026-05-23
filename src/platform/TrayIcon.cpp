#include "platform/TrayIcon.h"

#include "app/Branding.h"
#include "core/StringUtil.h"

namespace maku::platform {

bool TrayIcon::Create(HWND hwnd, HINSTANCE inst) {
    hwnd_ = hwnd;
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_USER + 1;
    nid_.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(brand::kAppIconResourceId));
    if (!nid_.hIcon) {
        const std::wstring iconPath =
            maku::util::GetExeDirectory() + L"\\assets\\icons\\MakuTweaker.ico";
        nid_.hIcon = static_cast<HICON>(LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, 0, 0,
                                                   LR_LOADFROMFILE | LR_DEFAULTSIZE));
    }
    wcscpy_s(nid_.szTip, brand::kDisplayName);
    return Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
}

void TrayIcon::Destroy() {
    Shell_NotifyIconW(NIM_DELETE, &nid_);
    if (nid_.hIcon) DestroyIcon(nid_.hIcon);
}

void TrayIcon::ShowBalloon(const std::wstring& title, const std::wstring& message) {
    NOTIFYICONDATAW n = nid_;
    n.uFlags = NIF_INFO;
    wcscpy_s(n.szInfoTitle, title.c_str());
    wcscpy_s(n.szInfo, message.c_str());
    n.dwInfoFlags = NIIF_WARNING;
    Shell_NotifyIconW(NIM_MODIFY, &n);
}

bool TrayIcon::HandleMessage(UINT msg, WPARAM, LPARAM) {
    if (msg == WM_USER + 1) return true;
    return false;
}

} // namespace maku::platform
