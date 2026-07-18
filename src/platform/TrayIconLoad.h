#pragma once

#include "app/Branding.h"
#include "core/EmbeddedStore.h"
#include "core/StringUtil.h"
#include <shellapi.h>
#include <windows.h>

namespace maku::platform::tray_icon {

inline HICON LoadIconFromFile(const std::wstring& path, int cx, int cy) {
    return reinterpret_cast<HICON>(
        LoadImageW(nullptr, path.c_str(), IMAGE_ICON, cx, cy, LR_LOADFROMFILE | LR_DEFAULTCOLOR));
}

/// Small icon for the notification area (16×16 / DPI-scaled).
inline HICON LoadNotifyAreaIcon(HINSTANCE inst) {
    const int cx = GetSystemMetrics(SM_CXSMICON);
    const int cy = GetSystemMetrics(SM_CYSMICON);

    HICON icon = reinterpret_cast<HICON>(LoadImageW(
        inst, MAKEINTRESOURCEW(brand::kAppIconResourceId), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
    if (icon) return icon;

    const std::wstring root = maku::embed::DataRoot();
    const wchar_t* names[] = {L"\\assets\\icons\\MakuT.ico",
                              L"\\assets\\icons\\MakuTweaker.ico"};
    for (const wchar_t* name : names) {
        icon = LoadIconFromFile(root + name, cx, cy);
        if (icon) return icon;
    }

    return reinterpret_cast<HICON>(
        LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON, cx, cy, LR_SHARED));
}

inline void SetNotifyIconVersion(NOTIFYICONDATAW& nid) {
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

} // namespace maku::platform::tray_icon
