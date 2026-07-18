#include "core/TweakProbe.h"
#include "core/Registry.h"
#include "core/OsUtil.h"
#include <windows.h>

namespace maku::tweak {
namespace {

bool DwordIs(HKEY root, const wchar_t* sub, const wchar_t* name, DWORD expected) {
    DWORD v{};
    return reg::GetDword(root, sub, name, v) && v == expected;
}

bool KeyExists(HKEY root, const wchar_t* sub) { return reg::KeyExists(root, sub); }

bool AnyNamespaceMissing() {
    const wchar_t* keys[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{A0953C92-50DC-43bf-BE83-3742FED03C9C}",
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{f86fa3ab-70d2-4fc7-9c99-fcbf05467f3a}"};
    for (auto k : keys)
        if (!reg::KeyExists(HKEY_LOCAL_MACHINE, k)) return true;
    return false;
}

bool WuServiceStopped() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE svc = OpenServiceW(scm, L"wuauserv", SERVICE_QUERY_STATUS);
    CloseServiceHandle(scm);
    if (!svc) return false;
    SERVICE_STATUS_PROCESS st{};
    bool stopped = false;
    if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&st),
                             sizeof(st), nullptr)) {
        stopped = st.dwCurrentState == SERVICE_STOPPED;
    }
    CloseServiceHandle(svc);
    return stopped;
}

} // namespace

ExplorerToggles ProbeExplorer() {
    ExplorerToggles t;
    t.nonremovable = AnyNamespaceMissing();
    DWORD v{};
    if (reg::GetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Hidden",
                      v))
        t.hidden = v == 1;
    if (reg::GetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                      L"HideFileExt", v))
        t.extensions = v == 0;
    if (reg::GetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"LaunchTo",
                      v))
        t.pcHome = v == 1;
    if (reg::GetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Classes\\CLSID\\{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}",
                      L"System.IsPinnedToNameSpaceTree", v))
        t.gallery = v == 0;
    if (reg::GetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel",
                      L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", v))
        t.showPc = v == 0;
    std::wstring shortcutTpl;
    t.shortcut = reg::GetString(
                     HKEY_CURRENT_USER,
                     L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\NamingTemplates",
                     L"ShortcutNameTemplate", shortcutTpl) &&
                 !shortcutTpl.empty();
    return t;
}

WindowsUpdateToggles ProbeWindowsUpdate() {
    WindowsUpdateToggles t;
    t.blockInternet =
        DwordIs(HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                L"DoNotConnectToWindowsUpdateInternetLocations", 1) ||
        DwordIs(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                L"DisableWindowsUpdateAccess", 1);
    t.excludeDrivers = DwordIs(HKEY_LOCAL_MACHINE,
                               L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                               L"ExcludeWUDriversInQualityUpdate", 1);
    t.serviceStopped = WuServiceStopped();
    t.disableReserves = DwordIs(HKEY_LOCAL_MACHINE,
                                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ReserveManager",
                                L"ShippedWithReserves", 0);
    return t;
}

SysRecToggles ProbeSysRec() {
    SysRecToggles t;
    t.telemetryOff = DwordIs(HKEY_LOCAL_MACHINE,
                             L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
                             L"AllowTelemetry", 0);
    t.uacOff = DwordIs(HKEY_LOCAL_MACHINE,
                       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA",
                       0);
    t.smartScreenOff = DwordIs(HKEY_LOCAL_MACHINE,
                               L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                               L"EnableSmartScreen", 0);
    t.bingOff = DwordIs(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Windows\\Explorer",
                        L"DisableSearchBoxSuggestions", 1);
    DWORD hib{};
    if (reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\Power", L"HibernateEnabled", hib))
        t.hibernateOff = hib == 0;
    return t;
}

PersonalizationToggles ProbePersonalization() {
    PersonalizationToggles t;
    t.darkTheme = DwordIs(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          L"AppsUseLightTheme", 0);
    t.transparencyOff = DwordIs(HKEY_CURRENT_USER,
                                L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                                L"EnableTransparency", 0);
    t.verboseBoot = DwordIs(HKEY_LOCAL_MACHINE,
                            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                            L"verbosestatus", 1);
    if (os::GetWindowsBuild() >= 22000) {
        t.endTask = DwordIs(HKEY_CURRENT_USER,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\TaskbarDeveloperSettings",
                            L"TaskbarEndTask", 1);
    }
    if (os::GetWindowsBuild() >= 22621) {
        t.oldContextMenu = KeyExists(
            HKEY_CURRENT_USER,
            L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32");
    }
    return t;
}

AdvancedToggles ProbeAdvanced() {
    AdvancedToggles t;
    t.vbsOff = DwordIs(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                       L"EnableVirtualizationBasedSecurity", 0);
    t.ttlReduced =
        DwordIs(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                L"DefaultTTL", 65);
    t.indexingOff =
        DwordIs(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\WSearch", L"Start", 4);
    return t;
}

} // namespace maku::tweak
