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

} // namespace maku::uwp
