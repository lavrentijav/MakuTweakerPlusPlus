#pragma once

#include "core/Registry.h"

#include <windows.h>

#include <string>

namespace maku::os {

bool IsElevated();
bool IsRunningAsAdmin();
/// Elevated UAC token or member of Administrators (for UI gating).
bool HasAdminRights();

void SetElevationOwner(HWND hwnd);

/// UAC once: relaunch the whole app elevated. Returns false (current instance exits) or user declined.
bool RequestElevation(HWND owner = nullptr);

/// Call before any admin-only UI action. If not elevated, prompts once to restart the whole app.
bool EnsureAdmin(HWND owner = nullptr);

inline bool EnsureMachineHiveAccess(HWND owner = nullptr) { return EnsureAdmin(owner); }

inline bool RequiresElevation(HKEY root) { return root == HKEY_LOCAL_MACHINE; }

inline int GetWindowsBuild() {
    // CurrentBuild / CurrentBuildNumber are REG_SZ, not REG_DWORD. Reading them
    // as a DWORD always fails and used to silently pin every machine to the
    // 19045 fallback, which broke build-gated tweaks on Windows 11.
    const wchar_t* kCurrentVersion = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    for (const wchar_t* name : {L"CurrentBuildNumber", L"CurrentBuild"}) {
        std::wstring text;
        if (!maku::reg::GetString(HKEY_LOCAL_MACHINE, kCurrentVersion, name, text)) continue;
        const int value = _wtoi(text.c_str());
        if (value > 0) return value;
    }
    DWORD build = 0;
    if (maku::reg::GetDword(HKEY_LOCAL_MACHINE, kCurrentVersion, L"CurrentBuild", build) &&
        build > 0)
        return static_cast<int>(build);
    return 19045;
}

inline bool IsWindowsSupported() { return GetWindowsBuild() >= 14393; }

} // namespace maku::os
