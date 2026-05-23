#include "core/Tweaks.h"

#include "core/LogFile.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include "core/StringUtil.h"

namespace maku::tweaks {
namespace {

proc::RunResult RunSys(const std::wstring& file, const std::wstring& args) {
    if (os::IsElevated()) return proc::Run(file, args);
    return proc::RunElevated(file, args);
}

proc::RunResult RunHiddenSys(const std::string& commandLine) {
    if (os::IsElevated()) return proc::RunHidden(commandLine);
    return proc::RunHiddenElevated(commandLine);
}

std::wstring EscapePsSingleQuoted(std::wstring s) {
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == L'\'') s.insert(i++, L"''");
    return s;
}

std::wstring BuildUwpRemovalScript(const std::vector<std::wstring>& patterns) {
    std::wstring script =
        L"$ErrorActionPreference='SilentlyContinue';"
        L"function Emit-MakuProgress([int]$cur,[int]$tot,[string]$name){"
        L"Write-Output (\"MAKU_PROGRESS|{0}|{1}|{2}\" -f $cur,$tot,$name)};"
        L"$patterns=@(";
    for (size_t i = 0; i < patterns.size(); ++i) {
        if (i) script += L",";
        script += L"'";
        script += EscapePsSingleQuoted(patterns[i]);
        script += L"'";
    }
    script +=
        L");"
        L"$apps=@();"
        L"foreach($pat in $patterns){"
        L"$apps+=Get-AppxPackage -AllUsers | Where-Object { $_.Name -like $pat -or $_.PackageFullName -like $pat };"
        L"};"
        L"$apps=$apps | Sort-Object PackageFullName -Unique;"
        L"$prov=@();"
        L"foreach($pat in $patterns){"
        L"$prov+=Get-AppxProvisionedPackage -Online | Where-Object { $_.DisplayName -like $pat -or $_.PackageName -like $pat };"
        L"};"
        L"$prov=$prov | Sort-Object PackageName -Unique;"
        L"$total=$apps.Count+$prov.Count;"
        L"if($total -eq 0){ Emit-MakuProgress 0 0 'none'; exit 0 };"
        L"$n=0;"
        L"foreach($p in $apps){"
        L"$n++;"
        L"Emit-MakuProgress $n $total $p.Name;"
        L"Remove-AppxPackage -Package $p.PackageFullName -AllUsers -ErrorAction SilentlyContinue;"
        L"};"
        L"foreach($pp in $prov){"
        L"$n++;"
        L"Emit-MakuProgress $n $total $pp.DisplayName;"
        L"Remove-AppxProvisionedPackage -Online -PackageName $pp.PackageName -ErrorAction SilentlyContinue;"
        L"};"
        L"Emit-MakuProgress $total $total 'done';"
        L"exit 0;";
    return script;
}

bool ParseMakuProgressLine(const std::string& line, int& cur, int& total, std::wstring& name) {
    if (line.rfind("MAKU_PROGRESS|", 0) != 0) return false;
    const std::string rest = line.substr(14);
    const size_t p1 = rest.find('|');
    const size_t p2 = rest.find('|', p1 + 1);
    if (p1 == std::string::npos || p2 == std::string::npos) return false;
    try {
        cur = std::stoi(rest.substr(0, p1));
        total = std::stoi(rest.substr(p1 + 1, p2 - p1 - 1));
        name = util::ToWide(rest.substr(p2 + 1));
    } catch (...) {
        return false;
    }
    return true;
}

} // namespace

void SetWindowsUpdatesEnabled(const bool enabled) {
    const wchar_t* wuPol = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
    if (!enabled) {
        logf::Info("Disable Windows Update");
        reg::SetDword(HKEY_LOCAL_MACHINE, wuPol, L"DoNotConnectToWindowsUpdateInternetLocations", 1);
        reg::SetDword(HKEY_LOCAL_MACHINE, wuPol, L"DisableWindowsUpdateAccess", 1);
        RunSys(L"net", L"stop wuauserv");
        RunSys(L"sc", L"config wuauserv start= disabled");
        RunHiddenSys(
            "echo.>>C:\\Windows\\System32\\drivers\\etc\\hosts && "
            "echo # MakuTweaker++ WU block>>C:\\Windows\\System32\\drivers\\etc\\hosts && "
            "echo 0.0.0.0 update.microsoft.com>>C:\\Windows\\System32\\drivers\\etc\\hosts && "
            "echo 0.0.0.0 windowsupdate.microsoft.com>>C:\\Windows\\System32\\drivers\\etc\\hosts");
    } else {
        logf::Info("Enable Windows Update");
        reg::DeleteKeyTree(HKEY_LOCAL_MACHINE, wuPol);
        RunSys(L"sc", L"config wuauserv start= demand");
        RunSys(L"net", L"start wuauserv");
    }
}

void ClearWindowsUpdateCache() {
    logf::Info("Clear Windows Update cache");
    RunSys(L"net", L"stop wuauserv");
    RunHiddenSys("net stop bits && net stop cryptsvc");
    RunHiddenSys("ren C:\\Windows\\SoftwareDistribution SoftwareDistribution.old 2>nul");
    RunSys(L"net", L"start wuauserv");
}

void ApplyTtlBypass() {
    logf::Info("TTL bypass: DefaultTTL=65");
    reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                  L"DefaultTTL", 65);
}

bool RemoveUwpPackages(const std::vector<std::wstring>& patterns, UwpProgressFn onProgress) {
    if (patterns.empty()) return false;
    if (!os::HasAdminRights()) {
        logf::Warn("Remove UWP: skipped (no administrator rights)");
        return false;
    }

    const std::wstring script = BuildUwpRemovalScript(patterns);
    logf::Info("Remove UWP packages (" + std::to_string(patterns.size()) + " patterns)");

    const auto onLine = [onProgress](const std::string& line) {
        if (!onProgress) return;
        int cur = 0, total = 0;
        std::wstring name;
        if (ParseMakuProgressLine(line, cur, total, name)) onProgress(cur, total, name);
    };

    proc::RunResult r;
    if (onProgress)
        r = proc::RunPowerShellAdminStreaming(script, onLine);
    else
        r = proc::RunPowerShellAdmin(script);

    if (r.exitCode == -1) {
        logf::Error("Remove UWP: failed to start PowerShell (see CreateProcess Win32 code above)");
        return false;
    }
    if (r.exitCode != 0) logf::Warn("Remove UWP: PowerShell exit " + std::to_string(r.exitCode));
    return true;
}

void DebloatUwp() {
    logf::Info("Debloat UWP packages");
    const std::vector<std::wstring> pkgs = {
        L"*zune*",       L"*skypeapp*", L"*bing*",      L"*copilot*",    L"*teams*",
        L"*xbox*",       L"*clipchamp*", L"*gamingapp*", L"*solitaire*", L"*onenote*",
        L"*officehub*",
    };
    RemoveUwpPackages(pkgs);
}

} // namespace maku::tweaks
