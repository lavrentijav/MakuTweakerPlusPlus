#pragma once

#include "app/Branding.h"
#include "core/EmbeddedData.h"
#include "core/StringUtil.h"
#include <shellapi.h>
#include <windows.h>

namespace maku::platform::tray_icon {

/// Small icon for the notification area (16×16 / DPI-scaled).
inline HICON LoadNotifyAreaIcon(HINSTANCE inst) {
    const int cx = GetSystemMetrics(SM_CXSMICON);
    const int cy = GetSystemMetrics(SM_CYSMICON);

    HICON icon = reinterpret_cast<HICON>(LoadImageW(
        inst, MAKEINTRESOURCEW(brand::kAppIconResourceId), IMAGE_ICON, cx, cy, LR_DEFAULTCOLOR));
    if (icon) return icon;

    const std::wstring iconPath =
        util::GetExeDirectory() + L"\\assets\\icons\\MakuTweaker.ico";
    icon = reinterpret_cast<HICON>(
        LoadImageW(nullptr, iconPath.c_str(), IMAGE_ICON, cx, cy, LR_LOADFROMFILE));
    if (icon) return icon;

#ifdef MAKU_EMBED_DATA
    if (const auto embedded = maku::embed::Get("assets/icons/MakuTweaker.ico")) {
        icon = CreateIconFromResourceEx(const_cast<BYTE*>(embedded->data),
                                        static_cast<DWORD>(embedded->size), TRUE, 0x00030000,
                                        cx, cy, LR_DEFAULTCOLOR);
        if (icon) return icon;
    }
#endif

    return reinterpret_cast<HICON>(
        LoadImageW(nullptr, IDI_APPLICATION, IMAGE_ICON, cx, cy, LR_SHARED));
}

inline void SetNotifyIconVersion(NOTIFYICONDATAW& nid) {
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
}

} // namespace maku::platform::tray_icon
