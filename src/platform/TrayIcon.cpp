#include "platform/TrayIcon.h"

#include "app/Branding.h"
#include "platform/MetricsTray.h"
#include "platform/TrayIconLoad.h"

namespace maku::platform {

bool TrayIcon::Create(HWND hwnd, HINSTANCE inst) {
    hwnd_ = hwnd;
#ifdef NOTIFYICONDATAW_V3_SIZE
    nid_.cbSize = NOTIFYICONDATAW_V3_SIZE;
#else
    nid_.cbSize = sizeof(nid_);
#endif
    nid_.hWnd = hwnd;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = WM_USER + 1;
    nid_.hIcon = tray_icon::LoadNotifyAreaIcon(inst);
    wcscpy_s(nid_.szTip, brand::kDisplayName);
    if (Shell_NotifyIconW(NIM_ADD, &nid_) == FALSE) return false;
    tray_icon::SetNotifyIconVersion(nid_);
    return true;
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

bool TrayIcon::HandleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_USER + 1) return ProcessCallback(hwnd_, wp, lp);
    return false;
}

bool TrayIcon::ProcessCallback(HWND hwnd, WPARAM wp, LPARAM lp) {
    (void)wp;
    return metrics_tray::ProcessMainTrayMessage(hwnd, wp, lp);
}

} // namespace maku::platform
