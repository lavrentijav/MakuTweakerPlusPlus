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
    DWORD build = 19045;
    maku::reg::GetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                        L"CurrentBuild", build);
    return static_cast<int>(build);
}

inline bool IsWindowsSupported() { return GetWindowsBuild() >= 14393; }

} // namespace maku::os
