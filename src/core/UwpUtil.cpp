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

const std::vector<std::wstring>& CuratedRemovablePackages() {
    // Mirrors the checkbox list on the original's "Remove UWP apps" page.
    // Deliberately includes things some people keep (Calculator, Notepad,
    // Store): the original presents them as options, not as a default.
    static const std::vector<std::wstring> packages = {
        L"Clipchamp.Clipchamp",
        L"Microsoft.3DBuilder",
        L"Microsoft.549981C3F5F10", // Cortana
        L"Microsoft.BingNews",
        L"Microsoft.BingWeather",
        L"Microsoft.Copilot",
        L"Microsoft.GamingApp",
        L"Microsoft.GetHelp",
        L"Microsoft.Getstarted",
        L"Microsoft.Messaging",
        L"Microsoft.Microsoft3DViewer",
        L"Microsoft.MicrosoftOfficeHub",
        L"Microsoft.MicrosoftSolitaireCollection",
        L"Microsoft.MicrosoftStickyNotes",
        L"Microsoft.MixedReality.Portal",
        L"Microsoft.MSPaint",
        L"Microsoft.Office.OneNote",
        L"Microsoft.OutlookForWindows",
        L"Microsoft.Paint",
        L"Microsoft.People",
        L"Microsoft.PowerAutomateDesktop",
        L"Microsoft.SkypeApp",
        L"Microsoft.Todos",
        L"Microsoft.Windows.DevHome",
        L"Microsoft.WindowsAlarms",
        L"Microsoft.WindowsCalculator",
        L"Microsoft.WindowsCamera",
        L"Microsoft.WindowsFeedbackHub",
        L"Microsoft.WindowsMaps",
        L"Microsoft.WindowsNotepad",
        L"Microsoft.WindowsSoundRecorder",
        L"Microsoft.WindowsStore",
        L"Microsoft.Xbox.TCUI",
        L"Microsoft.XboxApp",
        L"Microsoft.XboxSpeechToTextOverlay",
        L"Microsoft.YourPhone",
        L"Microsoft.ZuneMusic",
        L"Microsoft.ZuneVideo",
        L"MicrosoftCorporationII.QuickAssist",
        L"microsoft.windowscommunicationsapps",
    };
    return packages;
}

bool IsCuratedRemovable(const std::wstring& packageFullName) {
    // A full name is "Family_Version_Arch__PublisherId", so match on the
    // segment before the first underscore rather than a substring — otherwise
    // "Microsoft.Paint" would also match "Microsoft.Paint3D".
    const size_t underscore = packageFullName.find(L'_');
    const std::wstring family =
        underscore == std::wstring::npos ? packageFullName : packageFullName.substr(0, underscore);

    for (const auto& candidate : CuratedRemovablePackages())
        if (_wcsicmp(family.c_str(), candidate.c_str()) == 0) return true;
    return false;
}

} // namespace maku::uwp
