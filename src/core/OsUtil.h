#pragma once
#include "core/Registry.h"
#include <string>

namespace maku::os {

inline int GetWindowsBuild() {
    DWORD build = 19045;
    maku::reg::GetDword(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild", build);
    return static_cast<int>(build);
}

inline bool IsWindowsSupported() { return GetWindowsBuild() >= 14393; }

} // namespace maku::os
