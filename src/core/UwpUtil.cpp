#include "core/UwpUtil.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace maku::uwp {
namespace {

std::vector<PackageInfo> ParsePackageJson(const std::string& jsonText) {
    std::vector<PackageInfo> out;
    if (jsonText.empty()) return out;
    try {
        auto j = nlohmann::json::parse(jsonText);
        auto parseOne = [&](const nlohmann::json& item) {
            if (!item.is_object()) return;
            PackageInfo p;
            if (item.contains("Name") && item["Name"].is_string())
                p.name = util::ToWide(item["Name"].get<std::string>());
            if (item.contains("PackageFullName") && item["PackageFullName"].is_string())
                p.packageFullName = util::ToWide(item["PackageFullName"].get<std::string>());
            if (!p.name.empty() && !p.packageFullName.empty()) out.push_back(std::move(p));
        };
        if (j.is_array()) {
            for (const auto& item : j) parseOne(item);
        } else if (j.is_object()) {
            parseOne(j);
        }
    } catch (...) {
    }
    std::sort(out.begin(), out.end(),
              [](const PackageInfo& a, const PackageInfo& b) {
                  return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
              });
    auto last = std::unique(out.begin(), out.end(),
                            [](const PackageInfo& a, const PackageInfo& b) {
                                return a.packageFullName == b.packageFullName;
                            });
    out.erase(last, out.end());
    return out;
}

std::wstring EscapePsSingleQuoted(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 4);
    for (wchar_t c : s) {
        if (c == L'\'') out += L"''";
        else out += c;
    }
    return out;
}

} // namespace

PackageListResult ListInstalledPackages() {
    PackageListResult result;
    const std::wstring script =
        L"$p=Get-AppxPackage|Sort-Object Name|Select-Object Name,PackageFullName;"
        L"if($p){$p|ConvertTo-Json -Compress}else{'[]'}";
    const auto r = proc::RunPowerShell(script);
    result.exitCode = r.exitCode;
    result.packages = ParsePackageJson(r.output);
    if (result.packages.empty()) {
        if (!r.output.empty())
            result.error = r.output.substr(0, 512);
        else if (!r.error.empty())
            result.error = r.error.substr(0, 512);
        else if (r.exitCode != 0)
            result.error = "PowerShell exit code " + std::to_string(r.exitCode);
    }
    return result;
}

bool RemovePackages(const std::vector<std::wstring>& packageFullNames) {
    if (packageFullNames.empty()) return true;
    std::wstring script;
    for (const auto& pkg : packageFullNames) {
        if (pkg.empty()) continue;
        script += L"Remove-AppxPackage -Package '";
        script += EscapePsSingleQuoted(pkg);
        script += L"' -ErrorAction SilentlyContinue; ";
    }
    const auto r = proc::RunPowerShell(script);
    return r.exitCode == 0;
}

} // namespace maku::uwp
