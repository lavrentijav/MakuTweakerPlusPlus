#pragma once
#include <string>
#include <vector>

namespace maku::uwp {

struct PackageInfo {
    std::wstring name;
    std::wstring packageFullName;
};

struct PackageListResult {
    std::vector<PackageInfo> packages;
    int exitCode = -1;
    std::string error;
};

PackageListResult ListInstalledPackages();
bool RemovePackages(const std::vector<std::wstring>& packageFullNames);

/// Package family names the original offers as removable on its UWP page.
/// Used to pre-tick the equivalent rows in the full installed-package list —
/// the port lists everything, this brings back the original's curated shortcut.
const std::vector<std::wstring>& CuratedRemovablePackages();

/// True when `packageFullName` belongs to one of the curated families.
bool IsCuratedRemovable(const std::wstring& packageFullName);

} // namespace maku::uwp
