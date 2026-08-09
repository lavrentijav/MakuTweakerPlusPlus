#include "core/TweakRegistry.h"

#include "core/Localization.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include "core/StringUtil.h"

#include <powrprof.h>

#include <algorithm>
#include <cwctype>

namespace maku::tweak {
namespace {

// HKEY_* are reinterpret_casts, so these cannot be constexpr.
const HKEY HKLM = HKEY_LOCAL_MACHINE;
const HKEY HKCU = HKEY_CURRENT_USER;

// --- shared key paths -------------------------------------------------------

const wchar_t* kExplorerAdvanced =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced";
const wchar_t* kExplorerPoliciesCU =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
const wchar_t* kPoliciesSystem =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
const wchar_t* kThemesPersonalize =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
const wchar_t* kWindowsUpdatePolicy = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate";
const wchar_t* kWindowsUpdateAu = L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate\\AU";
const wchar_t* kDeviceGuard = L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard";
const wchar_t* kDeviceGuardScenarios = L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios";
const wchar_t* kMemoryManagement =
    L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Memory Management";

std::string Lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

// --- bcdedit ----------------------------------------------------------------

std::string BcdEnum(const wchar_t* store) {
    return Lower(proc::Run(L"bcdedit", std::wstring(L"/enum ") + store).output);
}

/// bcdedit prints "<name>    <value>" and localizes the value ("Yes"/"Да"),
/// so treat anything that is not an explicit no/0/false as enabled — the same
/// rule the original uses.
bool BcdFlagEnabled(const std::string& enumOutput, std::initializer_list<const char*> names) {
    size_t pos = 0;
    while (pos < enumOutput.size()) {
        size_t eol = enumOutput.find('\n', pos);
        if (eol == std::string::npos) eol = enumOutput.size();
        const std::string line = enumOutput.substr(pos, eol - pos);
        pos = eol + 1;

        bool hit = false;
        for (const char* n : names)
            if (line.find(n) != std::string::npos) hit = true;
        if (!hit) continue;

        // Last whitespace-separated token is the value.
        size_t end = line.find_last_not_of(" \t\r");
        if (end == std::string::npos) continue;
        size_t start = line.find_last_of(" \t", end);
        const std::string value =
            start == std::string::npos ? line.substr(0, end + 1) : line.substr(start + 1, end - start);
        if (value.empty()) continue;
        if (value == "0" || value == "false") return false;
        // "no" (en) and "\xd0\xbd\xd0\xb5\xd1\x82" (ru) both start with n/н.
        if (value[0] == 'n' || value.rfind("\xd0\xbd", 0) == 0) return false;
        return true;
    }
    return false;
}

void BcdSetFlag(const wchar_t* store, const wchar_t* name, bool on) {
    std::wstring args;
    if (on)
        args = std::wstring(L"/set ") + store + L" " + name + L" true";
    else
        args = std::wstring(L"/deletevalue ") + store + L" " + name;
    proc::Run(L"bcdedit", args);
}

// --- power schemes ----------------------------------------------------------
//
// powercfg's console output is localized ("Current AC Power Setting Index" vs
// "Текущий индекс..."), so scraping it only works on English systems. The
// PowrProf API returns the same data as numbers and costs no process spawn,
// which also keeps the app quiet while a page polls these values.

// Declared locally rather than relying on winnt.h's DEFINE_GUID, whose symbols
// need INITGUID and are easy to get wrong across SDK versions.
const GUID kProcessorSubgroup = {
    0x54533251, 0x82be, 0x4824, {0x96, 0xc1, 0x47, 0xb6, 0x0b, 0x74, 0x0d, 0x00}};
const GUID kProcThrottleMax = {
    0xbc5038f7, 0x23e0, 0x4960, {0x96, 0xda, 0x33, 0xab, 0xaf, 0x59, 0x35, 0xec}};
const GUID kVideoSubgroup = {
    0x7516b95f, 0xf776, 0x4464, {0x8c, 0x53, 0x06, 0x16, 0x7f, 0x40, 0xcc, 0x99}};
const GUID kVideoPowerdownTimeout = {
    0x3c0bc021, 0xc8a8, 0x4e07, {0xa9, 0x73, 0x6b, 0x14, 0xcb, 0xcb, 0x2b, 0x7e}};
const GUID kSleepSubgroup = {
    0x238c9fa8, 0x0aad, 0x41ed, {0x83, 0xf4, 0x97, 0xbe, 0x24, 0x2c, 0x8f, 0x20}};
const GUID kStandbyTimeout = {
    0x29f6c1db, 0x86da, 0x48c5, {0x9f, 0xdb, 0xf2, 0xb6, 0x7b, 0x1f, 0x44, 0xda}};
const GUID kUltimateTemplate = {
    0xe9a42b02, 0xd5df, 0x448d, {0xaa, 0x00, 0x03, 0xf1, 0x47, 0x49, 0xeb, 0x61}};
const GUID kBalancedScheme = {
    0x381b4222, 0xf694, 0x41f0, {0x96, 0x85, 0xff, 0x5b, 0xb2, 0x60, 0xdf, 0x2e}};

bool IsZeroGuid(const GUID& g) {
    static const GUID zero{};
    return IsEqualGUID(g, zero) != FALSE;
}

/// Active scheme GUID, or a zeroed GUID when the query fails.
GUID ActiveSchemeGuid() {
    GUID* active = nullptr;
    if (PowerGetActiveScheme(nullptr, &active) != ERROR_SUCCESS || !active) return GUID{};
    const GUID copy = *active;
    LocalFree(active);
    return copy;
}

std::wstring GuidToString(const GUID& g) {
    wchar_t buf[64]{};
    StringFromGUID2(g, buf, static_cast<int>(std::size(buf)));
    std::wstring s = buf;
    // StringFromGUID2 wraps the value in braces; callers want it bare.
    if (s.size() >= 2 && s.front() == L'{') s = s.substr(1, s.size() - 2);
    return s;
}

/// -1 when the value cannot be read.
int ReadAcIndex(const GUID& subgroup, const GUID& setting) {
    const GUID scheme = ActiveSchemeGuid();
    DWORD value = 0;
    if (PowerReadACValueIndex(nullptr, &scheme, &subgroup, &setting, &value) != ERROR_SUCCESS)
        return -1;
    return static_cast<int>(value);
}

bool WriteIndexBoth(const GUID& subgroup, const GUID& setting, DWORD ac, DWORD dc) {
    GUID scheme = ActiveSchemeGuid();
    const bool okAc =
        PowerWriteACValueIndex(nullptr, &scheme, &subgroup, &setting, ac) == ERROR_SUCCESS;
    const bool okDc =
        PowerWriteDCValueIndex(nullptr, &scheme, &subgroup, &setting, dc) == ERROR_SUCCESS;
    // Windows only publishes written values once the scheme is re-activated.
    PowerSetActiveScheme(nullptr, &scheme);
    return okAc && okDc;
}

std::wstring SchemeFriendlyName(const GUID& scheme) {
    GUID query = scheme;
    DWORD bytes = 0;
    if (PowerReadFriendlyName(nullptr, &query, nullptr, nullptr, nullptr, &bytes) != ERROR_SUCCESS)
        return {};
    std::vector<BYTE> buffer(bytes + sizeof(wchar_t), 0);
    if (PowerReadFriendlyName(nullptr, &query, nullptr, nullptr, buffer.data(), &bytes) !=
        ERROR_SUCCESS)
        return {};
    return reinterpret_cast<const wchar_t*>(buffer.data());
}

std::vector<GUID> EnumeratePowerSchemes() {
    std::vector<GUID> out;
    for (DWORD index = 0;; ++index) {
        GUID guid{};
        DWORD size = sizeof(guid);
        if (PowerEnumerate(nullptr, nullptr, nullptr, ACCESS_SCHEME, index,
                           reinterpret_cast<UCHAR*>(&guid), &size) != ERROR_SUCCESS)
            break;
        out.push_back(guid);
    }
    return out;
}

/// The Ultimate Performance scheme, creating it when Windows hides it.
/// Returns a zeroed GUID if this edition will not provide it.
GUID EnsureUltimatePerformanceScheme() {
    const std::wstring templateName = SchemeFriendlyName(kUltimateTemplate);
    for (const GUID& scheme : EnumeratePowerSchemes()) {
        if (IsEqualGUID(scheme, kUltimateTemplate)) return scheme;
        if (!templateName.empty() && SchemeFriendlyName(scheme) == templateName) return scheme;
    }

    GUID source = kUltimateTemplate;
    GUID* created = nullptr;
    if (PowerDuplicateScheme(nullptr, &source, &created) == ERROR_SUCCESS && created) {
        const GUID copy = *created;
        LocalFree(created);
        return copy;
    }
    return GUID{};
}

bool UltimatePerformanceActive() {
    const GUID active = ActiveSchemeGuid();
    if (IsEqualGUID(active, kUltimateTemplate)) return true;
    const std::wstring templateName = SchemeFriendlyName(kUltimateTemplate);
    return !templateName.empty() && SchemeFriendlyName(active) == templateName;
}

// --- Explorer "This PC" namespace folders -----------------------------------

/// Every MyComputer\NameSpace CLSID the original hides, in both native and
/// WOW6432 views.
const wchar_t* const kNamespaceClsids[] = {
    L"{A0953C92-50DC-43bf-BE83-3742FED03C9C}", L"{f86fa3ab-70d2-4fc7-9c99-fcbf05467f3a}",
    L"{A8CDFF1C-4878-43be-B5FD-F8091C1C60D0}", L"{d3162b92-9365-467a-956b-92703aca08af}",
    L"{374DE290-123F-4565-9164-39C4925E467B}", L"{088e3905-0323-4b02-9826-5d99428e115f}",
    L"{3ADD1653-EB32-4cb0-BBD7-DFA0ABB5ACCA}", L"{24ad3ad4-a569-4530-98e1-ab02f9417aa8}",
    L"{1CF1260C-4DD0-4ebb-811F-33C572699FDE}", L"{3dfdf296-dbec-4fb4-81d1-6a3438bcf4de}",
    L"{B4BFCC3A-DB2C-424C-B029-7FE99A87C641}", L"{0DB7E03F-FC29-4DC6-9020-FF41B59E513A}",
};

std::wstring NamespacePath(bool wow64, const wchar_t* clsid) {
    std::wstring p = wow64 ? L"SOFTWARE\\Wow6432Node\\Microsoft" : L"SOFTWARE\\Microsoft";
    p += L"\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\";
    p += clsid;
    return p;
}

bool NamespaceFoldersHidden() {
    for (const wchar_t* clsid : kNamespaceClsids)
        for (bool wow : {false, true})
            if (!reg::KeyExists(HKLM, NamespacePath(wow, clsid))) return true;
    return false;
}

void SetNamespaceFoldersHidden(bool hidden) {
    for (const wchar_t* clsid : kNamespaceClsids) {
        for (bool wow : {false, true}) {
            const std::wstring path = NamespacePath(wow, clsid);
            if (hidden)
                reg::DeleteKeyTree(HKLM, path);
            else
                reg::CreateKey(HKLM, path);
        }
    }
}

const wchar_t* const kDelegateFolders[] = {
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\DelegateFolders"
    L"\\{F5FB2C77-0E2F-4A16-A381-3E560C68BC83}",
    L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace"
    L"\\DelegateFolders\\{F5FB2C77-0E2F-4A16-A381-3E560C68BC83}",
};

// --- Windows Update service pinning -----------------------------------------

struct ServiceStart {
    const wchar_t* name;
    DWORD enabled; ///< Windows default Start value.
};

/// Services the original disables to hard-block Windows Update. `enabled`
/// restores each one's stock start type rather than a blanket 2 — getting this
/// wrong is what leaves BITS auto-starting on a repaired machine.
constexpr ServiceStart kUpdateServices[] = {
    {L"wuauserv", 3}, {L"UsoSvc", 2}, {L"WaaSMedicSvc", 3}, {L"dosvc", 2}, {L"bits", 3},
};

std::wstring ServiceKey(const wchar_t* name) {
    return std::wstring(L"SYSTEM\\CurrentControlSet\\Services\\") + name;
}

// --- telemetry key set ------------------------------------------------------

struct DwordTweakValue {
    HKEY root;
    const wchar_t* key;
    const wchar_t* name;
    DWORD on;
    DWORD off;
};

const DwordTweakValue kTelemetryValues[] = {
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"AllowTelemetry", 0, 1},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"AllowTelemetry", 0, 1},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"MaxTelemetryAllowed", 0, 1},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\DataCollection", L"DoNotShowFeedbackNotifications", 1, 0},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry", 0, 1},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows NT\\CurrentVersion\\Software Protection Platform", L"NoGenTicket", 1, 0},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", L"AITEnable", 0, 1},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", L"AllowTelemetry", 0, 1},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", L"DisableEngine", 1, 0},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", L"DisableInventory", 1, 0},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", L"DisablePCA", 1, 0},
    {HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\AppCompat", L"DisableUAR", 1, 0},
};

// --- accessibility "sticky keys" trio ---------------------------------------

struct FlagsPair {
    const wchar_t* key;
    const wchar_t* on;
    const wchar_t* off;
};

constexpr FlagsPair kStickyKeyFlags[] = {
    {L"Control Panel\\Accessibility\\StickyKeys", L"506", L"510"},
    {L"Control Panel\\Accessibility\\Keyboard Response", L"122", L"126"},
    {L"Control Panel\\Accessibility\\ToggleKeys", L"58", L"62"},
};

// ---------------------------------------------------------------------------

/// Toggle backed by one DWORD. `onValue`/`offValue` mirror the original's
/// ternaries so an "off" always restores the Windows default rather than
/// deleting the value.
Tweak DwordToggle(const char* id, const char* page, const char* locCategory, const char* locKey,
                  const char* summary, HKEY root, const wchar_t* key, const wchar_t* name,
                  DWORD onValue, DWORD offValue, Reboot reboot = Reboot::None) {
    Tweak t{};
    t.id = id;
    t.page = page;
    t.locCategory = locCategory;
    t.locSection = "main";
    t.locKey = locKey;
    t.summary = summary;
    t.kind = Kind::Toggle;
    t.reboot = reboot;
    t.needsAdmin = (root == HKLM);
    t.probe = [root, key, name, onValue] { return reg::DwordEquals(root, key, name, onValue); };
    t.apply = [root, key, name, onValue, offValue](bool on) {
        reg::SetDword(root, key, name, on ? onValue : offValue);
    };
    return t;
}

Tweak Action(const char* id, const char* page, const char* locCategory, const char* locKey,
             const char* summary, std::function<void()> fn, Reboot reboot = Reboot::None,
             bool admin = true) {
    Tweak t{};
    t.id = id;
    t.page = page;
    t.locCategory = locCategory;
    t.locSection = "main";
    t.locKey = locKey;
    t.summary = summary;
    t.kind = Kind::Action;
    t.reboot = reboot;
    t.needsAdmin = admin;
    t.action = std::move(fn);
    return t;
}

// ---------------------------------------------------------------------------

std::vector<Tweak> BuildTable() {
    std::vector<Tweak> t;
    t.reserve(80);

    // ===================== Explorer & Desktop =====================

    {
        Tweak x{};
        x.id = "explorer.hide-nonremovable";
        x.page = "exp";
        x.locCategory = "expl";
        x.locSection = "main";
        x.locKey = "nonremovable";
        x.summary = "Hide non-removable library folders in This PC";
        x.needsAdmin = true;
        x.maxBuild = 22620; // The original hides this switch on 22621+.
        x.reboot = Reboot::ExplorerRestart;
        x.probe = [] { return NamespaceFoldersHidden(); };
        x.apply = [](bool on) { SetNamespaceFoldersHidden(on); };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("explorer.show-hidden", "exp", "expl", "hidden",
                            "Show hidden files and folders", HKCU, kExplorerAdvanced, L"Hidden", 1,
                            0, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.show-extensions", "exp", "expl", "ext",
                            "Show file extensions", HKCU, kExplorerAdvanced, L"HideFileExt", 0, 1,
                            Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.open-this-pc", "exp", "expl", "pchome",
                            "Open Explorer on This PC instead of Home", HKCU, kExplorerAdvanced,
                            L"LaunchTo", 1, 2, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.hide-gallery", "exp", "expl", "gallery",
                            "Hide the Gallery section", HKCU,
                            L"SOFTWARE\\Classes\\CLSID\\{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}",
                            L"System.IsPinnedToNameSpaceTree", 0, 1, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.show-this-pc-desktop", "exp", "expl", "showpc",
                            "Show This PC on the desktop", HKCU,
                            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer"
                            L"\\HideDesktopIcons\\NewStartPanel",
                            L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", 0, 1,
                            Reboot::ExplorerRestart));

    {
        Tweak x{};
        x.id = "explorer.clean-shortcut-name";
        x.page = "exp";
        x.locCategory = "expl";
        x.locSection = "main";
        x.locKey = "shortcut";
        x.summary = "Drop the \" - Shortcut\" suffix from new shortcuts";
        x.reboot = Reboot::ExplorerRestart;
        const wchar_t* kNaming =
            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\NamingTemplates";
        x.probe = [kNaming] {
            return reg::StringEquals(HKCU, kNaming, L"ShortcutNameTemplate", L"%s.lnk");
        };
        x.apply = [kNaming](bool on) {
            if (on)
                reg::SetString(HKCU, kNaming, L"ShortcutNameTemplate", L"%s.lnk");
            else
                reg::DeleteKeyTree(HKCU, kNaming);
        };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("explorer.hide-frequent", "exp", "expl", "quickfreq",
                            "Hide frequent folders in Quick access", HKCU,
                            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                            L"ShowFrequent", 0, 1, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.item-checkboxes", "exp", "expl", "checkboxes",
                            "Show item checkboxes", HKCU, kExplorerAdvanced, L"AutoCheckSelect", 1,
                            0, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.no-recent-docs", "exp", "expl", "recdocs",
                            "Stop recording recently opened documents", HKCU, kExplorerPoliciesCU,
                            L"NoRecentDocsHistory", 1, 0, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("explorer.confirm-delete", "exp", "expl", "confirmdel",
                            "Ask for confirmation before deleting files", HKCU,
                            kExplorerPoliciesCU, L"ConfirmFileDelete", 1, 0,
                            Reboot::ExplorerRestart));

    t.push_back(Action("explorer.fix-delegate-folders", "exp", "expl", "e8b",
                       "Remove the duplicate removable-drive entries in This PC",
                       [] {
                           for (const wchar_t* k : kDelegateFolders) reg::DeleteKeyTree(HKLM, k);
                       },
                       Reboot::ExplorerRestart));

    t.push_back(Action("explorer.show-all-drives", "exp", "expl", "showall",
                       "Unhide every drive letter",
                       [] {
                           reg::SetDword(HKCU,
                                         L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies"
                                         L"\\Explorer",
                                         L"NoDrives", 0);
                       },
                       Reboot::ExplorerRestart, false));

    // ===================== Windows Update =====================

    {
        Tweak x{};
        x.id = "wu.block-updates";
        x.page = "wu";
        x.locCategory = "wu";
        x.locSection = "main";
        x.locKey = "wu1";
        x.summary = "Block Windows Update completely (policy + services)";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] { return reg::DwordEquals(HKLM, kWindowsUpdateAu, L"NoAutoUpdate", 1); };
        // The original could only ever turn this on; the off path below restores
        // every policy value and service start type it changed.
        x.apply = [](bool on) {
            reg::SetDword(HKLM, kWindowsUpdatePolicy,
                          L"DoNotConnectToWindowsUpdateInternetLocations", on ? 1 : 0);
            reg::SetDword(HKLM, kWindowsUpdatePolicy, L"DisableWindowsUpdateAccess", on ? 1 : 0);
            reg::SetDword(HKLM, kWindowsUpdatePolicy, L"DisableDualScan", on ? 1 : 0);
            reg::SetDword(HKLM, kWindowsUpdateAu, L"NoAutoUpdate", on ? 1 : 0);
            for (const auto& svc : kUpdateServices)
                reg::SetDword(HKLM, ServiceKey(svc.name), L"Start", on ? 4 : svc.enabled);
        };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("wu.exclude-drivers", "wu", "wu", "wu3",
                            "Exclude drivers from quality updates", HKLM, kWindowsUpdatePolicy,
                            L"ExcludeWUDriversInQualityUpdate", 1, 0));
    t.push_back(DwordToggle("wu.disable-reserves", "wu", "wu", "wu6",
                            "Release reserved storage", HKLM,
                            L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ReserveManager",
                            L"ShippedWithReserves", 0, 1, Reboot::Restart));

    t.push_back(Action("wu.pause-updates", "wu", "wu", "wu5",
                       "Pause updates until 2077 via the Update UX settings",
                       [] {
                           const wchar_t* ux = L"SOFTWARE\\Microsoft\\WindowsUpdate\\UX\\Settings";
                           reg::SetDword(HKLM, ux, L"ActiveHoursStart", 9);
                           reg::SetDword(HKLM, ux, L"ActiveHoursEnd", 2);
                           for (const wchar_t* n : {L"PauseFeatureUpdatesStartTime",
                                                    L"PauseQualityUpdatesStartTime",
                                                    L"PauseUpdatesStartTime"})
                               reg::SetString(HKLM, ux, n, L"2015-01-01T00:00:00Z");
                           for (const wchar_t* n : {L"PauseFeatureUpdatesEndTime",
                                                    L"PauseQualityUpdatesEndTime",
                                                    L"PauseUpdatesExpiryTime"})
                               reg::SetString(HKLM, ux, n, L"2077-01-01T00:00:00Z");
                       }));

    t.push_back(Action("wu.clear-cache", "wu", "wu", "wu4",
                       "Reset the Windows Update download cache",
                       [] {
                           proc::RunHidden("net stop wuauserv");
                           proc::RunHidden("net stop bits");
                           proc::RunHidden("net stop cryptsvc");
                           proc::RunHidden(
                               "cmd /c ren %SystemRoot%\\SoftwareDistribution "
                               "SoftwareDistribution.old");
                           proc::RunHidden("net start cryptsvc");
                           proc::RunHidden("net start bits");
                           proc::RunHidden("net start wuauserv");
                       },
                       Reboot::Restart));

    // ===================== System & Recovery =====================

    {
        Tweak x{};
        x.id = "sys.telemetry-off";
        x.page = "sys";
        x.locCategory = "sr";
        x.locSection = "main";
        x.locKey = "telemetry";
        x.summary = "Disable Windows telemetry and app-compat reporting";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] {
            return reg::DwordEquals(HKLM, kTelemetryValues[0].key, L"AllowTelemetry", 0);
        };
        x.apply = [](bool on) {
            for (const auto& v : kTelemetryValues)
                reg::SetDword(v.root, v.key, v.name, on ? v.on : v.off);
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "sys.uac-off";
        x.page = "sys";
        x.locCategory = "sr";
        x.locSection = "main";
        x.locKey = "uac";
        x.summary = "Disable User Account Control prompts";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] { return reg::DwordEquals(HKLM, kPoliciesSystem, L"EnableLUA", 0); };
        x.apply = [](bool on) {
            reg::SetDword(HKLM, kPoliciesSystem, L"EnableLUA", on ? 0 : 1);
            if (on) {
                reg::SetDword(HKCU,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Attachments",
                              L"SaveZoneInformation", 1);
                reg::SetString(
                    HKCU, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Associations",
                    L"LowRiskFileTypes", L".exe;.msi;.bat;");
            }
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "sys.hibernate-off";
        x.page = "sys";
        x.locCategory = "sr";
        x.locSection = "main";
        x.locKey = "hybern";
        x.summary = "Disable hibernation and delete hiberfil.sys";
        x.needsAdmin = true;
        x.probe = [] {
            return reg::DwordEquals(HKLM, L"SYSTEM\\CurrentControlSet\\Control\\Power",
                                    L"HibernateEnabled", 0);
        };
        x.apply = [](bool on) { proc::Run(L"powercfg", on ? L"/hibernate off" : L"/hibernate on"); };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "sys.smartscreen-off";
        x.page = "sys";
        x.locCategory = "sr";
        x.locSection = "main";
        x.locKey = "smartscreen";
        x.summary = "Disable SmartScreen filtering";
        x.needsAdmin = true;
        x.probe = [] {
            return reg::DwordEquals(HKLM, kPoliciesSystem, L"EnableSmartScreen", 0) ||
                   reg::StringEquals(HKLM,
                                     L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                                     L"SmartScreenEnabled", L"Off");
        };
        x.apply = [](bool on) {
            reg::SetDword(HKLM, kPoliciesSystem, L"EnableSmartScreen", on ? 0 : 1);
            reg::SetString(HKLM, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer",
                           L"SmartScreenEnabled", on ? L"Off" : L"Warn");
            reg::SetDword(HKCU,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Attachments",
                          L"SaveZoneInformation", on ? 1 : 0);
        };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("sys.bing-search-off", "sys", "sr", "bing",
                            "Remove Bing results from Start search", HKCU,
                            L"Software\\Policies\\Microsoft\\Windows\\Explorer",
                            L"DisableSearchBoxSuggestions", 1, 0, Reboot::ExplorerRestart));

    {
        Tweak x{};
        x.id = "sys.sticky-keys-off";
        x.page = "sys";
        x.locCategory = "sr";
        x.locSection = "main";
        x.locKey = "sticky";
        x.summary = "Stop the Sticky/Filter/Toggle Keys shortcut prompts";
        x.probe = [] {
            for (const auto& f : kStickyKeyFlags)
                if (reg::StringEquals(HKCU, f.key, L"Flags", f.on)) return true;
            return false;
        };
        x.apply = [](bool on) {
            for (const auto& f : kStickyKeyFlags)
                reg::SetString(HKCU, f.key, L"Flags", on ? f.on : f.off);
        };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("sys.core-isolation-off", "sys", "sr", "coreisol",
                            "Disable memory integrity (core isolation)", HKLM,
                            kDeviceGuardScenarios, L"HypervisorEnforcedCodeIntegrity", 0, 1,
                            Reboot::Restart));
    t.push_back(DwordToggle("sys.chkdsk-timeout", "sys", "sr", "chkdsk",
                            "Raise the chkdsk countdown to 60 seconds", HKLM,
                            L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                            L"AutoChkTimeout", 60, 8));
    t.push_back(DwordToggle("sys.block-bitlocker", "sys", "sr", "bitlocker",
                            "Prevent automatic device encryption", HKLM,
                            L"SYSTEM\\CurrentControlSet\\Control\\BitLocker",
                            L"PreventDeviceEncryption", 1, 0, Reboot::Restart));

    t.push_back(Action("sys.sfc-scan", "sys", "sr", "sfclabel", "Run sfc /scannow",
                       [] { proc::Run(L"sfc", L"/scannow"); }, Reboot::AfterScan));
    t.push_back(Action("sys.dism-restore", "sys", "sr", "dismlabel",
                       "Run DISM /RestoreHealth",
                       [] { proc::Run(L"dism", L"/Online /Cleanup-Image /RestoreHealth"); },
                       Reboot::AfterScan));
    t.push_back(Action("sys.clean-temp", "sys", "sr", "templabel", "Delete temporary files",
                       [] {
                           proc::RunHidden("cmd /c del /q /f /s \"%TEMP%\\*\"");
                           proc::RunHidden("cmd /c del /q /f /s \"%SystemRoot%\\Temp\\*\"");
                       },
                       Reboot::None, false));
    t.push_back(Action("sys.battery-report", "sys", "sr", "batterylabel",
                       "Write a battery health report to the user profile",
                       [] { proc::RunHidden("cmd /c powercfg /batteryreport /output \"%USERPROFILE%\\battery-report.html\""); },
                       Reboot::None, false));
    t.push_back(Action("sys.clean-pip-cache", "sys", "sr", "pipcache", "Clear the pip cache",
                       [] {
                           proc::RunPowerShell(
                               L"Remove-Item -Recurse -Force \"$env:LOCALAPPDATA\\pip\\cache\" "
                               L"-ErrorAction SilentlyContinue");
                       },
                       Reboot::None, false));
    t.push_back(Action("sys.empty-recycle-bin", "sys", "sr", "recyclebinlabel", "Empty the Recycle Bin",
                       [] {
                           proc::RunPowerShell(L"Clear-RecycleBin -Force -ErrorAction "
                                               L"SilentlyContinue");
                       },
                       Reboot::None, false));
    t.push_back(Action("sys.clean-thumbnail-cache", "sys", "sr", "thumbcachelabel",
                       "Clear the Explorer thumbnail cache",
                       [] {
                           proc::KillExplorer();
                           proc::RunHidden(
                               "cmd /c del /f /s /q /a \"%LOCALAPPDATA%\\Microsoft\\Windows"
                               "\\Explorer\\thumbcache_*.db\"");
                           proc::RestartExplorer();
                       },
                       Reboot::None, false));

    // ===================== Personalization =====================

    {
        Tweak x{};
        x.id = "per.dark-theme";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "darktheme";
        x.summary = "Use the dark Windows theme";
        x.probe = [] {
            return reg::DwordEquals(HKCU, kThemesPersonalize, L"AppsUseLightTheme", 0) &&
                   reg::DwordEquals(HKCU, kThemesPersonalize, L"SystemUsesLightTheme", 0);
        };
        x.apply = [](bool on) {
            reg::SetDword(HKCU, kThemesPersonalize, L"AppsUseLightTheme", on ? 0 : 1);
            reg::SetDword(HKCU, kThemesPersonalize, L"SystemUsesLightTheme", on ? 0 : 1);
        };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("per.transparency-off", "per", "per", "transparency",
                            "Disable window transparency", HKCU, kThemesPersonalize,
                            L"EnableTransparency", 0, 1));
    t.push_back(DwordToggle("per.verbose-boot", "per", "per", "verbose",
                            "Show verbose startup/shutdown status messages", HKLM, kPoliciesSystem,
                            L"verbosestatus", 1, 0, Reboot::Restart));
    t.push_back(DwordToggle("per.disable-logon-acrylic", "per", "per", "blur",
                            "Disable the blurred logon background", HKLM,
                            L"SOFTWARE\\Policies\\Microsoft\\Windows\\System",
                            L"DisableAcrylicBackgroundOnLogon", 1, 0, Reboot::Restart));

    {
        Tweak x{};
        x.id = "per.end-task";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "etask";
        x.summary = "Add End Task to the taskbar right-click menu";
        x.minBuild = 22000;
        const wchar_t* kDev = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced"
                              L"\\TaskbarDeveloperSettings";
        x.probe = [kDev] { return reg::DwordEquals(HKCU, kDev, L"TaskbarEndTask", 1); };
        x.apply = [kDev](bool on) { reg::SetDword(HKCU, kDev, L"TaskbarEndTask", on ? 1 : 0); };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "per.old-context-menu";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "oldcont";
        x.summary = "Restore the Windows 10 context menu";
        x.minBuild = 22621;
        x.reboot = Reboot::ExplorerRestart;
        const wchar_t* kClsid =
            L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}";
        x.probe = [kClsid] {
            return reg::KeyExists(HKCU, std::wstring(kClsid) + L"\\InprocServer32");
        };
        x.apply = [kClsid](bool on) {
            if (on)
                reg::SetString(HKCU, std::wstring(kClsid) + L"\\InprocServer32", L"", L"");
            else
                reg::DeleteKeyTree(HKCU, kClsid);
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "per.fast-context-menu";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "delcont";
        x.summary = "Shorten the menu open delay to 50 ms";
        x.reboot = Reboot::Restart;
        x.probe = [] {
            return reg::StringEquals(HKCU, L"Control Panel\\Desktop", L"MenuShowDelay", L"50");
        };
        x.apply = [](bool on) {
            reg::SetString(HKCU, L"Control Panel\\Desktop", L"MenuShowDelay", on ? L"50" : L"400");
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "per.small-title-bars";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "smallwindows";
        x.summary = "Use compact window title bars";
        x.reboot = Reboot::Restart;
        const wchar_t* kMetrics = L"Control Panel\\Desktop\\WindowMetrics";
        x.probe = [kMetrics] {
            return reg::StringEquals(HKCU, kMetrics, L"CaptionHeight", L"-270");
        };
        x.apply = [kMetrics](bool on) {
            const wchar_t* v = on ? L"-270" : L"-330";
            reg::SetString(HKCU, kMetrics, L"CaptionHeight", v);
            reg::SetString(HKCU, kMetrics, L"CaptionWidth", v);
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "per.no-boot-logo";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "disablelogo";
        x.summary = "Hide the Windows logo during boot";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] {
            return BcdFlagEnabled(BcdEnum(L"{globalsettings}"),
                                  {"custom:16000067", "bootlogo", "nobootlogo"});
        };
        x.apply = [](bool on) { BcdSetFlag(L"{globalsettings}", L"custom:16000067", on); };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "per.no-boot-animation";
        x.page = "per";
        x.locCategory = "per";
        x.locSection = "main";
        x.locKey = "disableanim";
        x.summary = "Hide the spinning dots during boot";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] {
            return BcdFlagEnabled(BcdEnum(L"{globalsettings}"),
                                  {"custom:16000069", "nobootuxprogress"});
        };
        x.apply = [](bool on) { BcdSetFlag(L"{globalsettings}", L"custom:16000069", on); };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "per.hide-taskbar-widgets";
        x.page = "per";
        x.locCategory = "quick";
        x.locSection = "main";
        x.locKey = "hidewidget";
        x.summary = "Hide Task View, Widgets and Chat from the taskbar";
        x.reboot = Reboot::ExplorerRestart;
        x.probe = [] { return reg::DwordEquals(HKCU, kExplorerAdvanced, L"TaskbarDa", 0); };
        x.apply = [](bool on) {
            for (const wchar_t* n : {L"ShowTaskViewButton", L"TaskbarDa", L"TaskbarMn"})
                reg::SetDword(HKCU, kExplorerAdvanced, n, on ? 0 : 1);
        };
        t.push_back(std::move(x));
    }

    t.push_back(DwordToggle("per.no-search-highlights", "per", "quick", "removeads",
                            "Remove search highlights and ads from the search box", HKCU,
                            L"Software\\Microsoft\\Windows\\CurrentVersion\\SearchSettings",
                            L"IsDynamicSearchBoxEnabled", 0, 1, Reboot::ExplorerRestart));
    t.push_back(DwordToggle("per.clipboard-history", "per", "quick", "clipboard",
                            "Enable clipboard history (Win+V)", HKCU,
                            L"SOFTWARE\\Microsoft\\Clipboard", L"EnableClipboardHistory", 1, 0));

    // ===================== Advanced =====================

    {
        Tweak x{};
        x.id = "adv.vbs-off";
        x.page = "adv";
        x.locCategory = "adv";
        x.locSection = "main";
        x.locKey = "vbs";
        x.summary = "Disable virtualization-based security";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] {
            return reg::DwordEquals(HKLM, kDeviceGuard, L"EnableVirtualizationBasedSecurity", 0);
        };
        x.apply = [](bool on) {
            reg::SetDword(HKLM, kDeviceGuard, L"EnableVirtualizationBasedSecurity", on ? 0 : 1);
            reg::SetDword(HKLM, kDeviceGuard, L"RequirePlatformSecurityFeatures", on ? 0 : 3);
            reg::SetDword(HKLM, kDeviceGuard, L"RequireMicrosoftSignedBootChain", on ? 0 : 1);
            reg::SetDword(HKLM, kDeviceGuard, L"KernelDMAProtection", on ? 0 : 1);
            reg::SetDword(HKLM, L"SYSTEM\\CurrentControlSet\\Control\\Lsa", L"LsaCfgFlags",
                          on ? 0 : 1);
            reg::SetDword(HKLM,
                          L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios"
                          L"\\HypervisorEnforcedCodeIntegrity",
                          L"Enabled", on ? 0 : 1);
            proc::Run(L"bcdedit",
                      on ? L"/set hypervisorlaunchtype off" : L"/set hypervisorlaunchtype auto");
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "adv.ttl-65";
        x.page = "adv";
        x.locCategory = "adv";
        x.locSection = "main";
        x.locKey = "ttl";
        x.summary = "Set the default IP TTL to 65";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        const wchar_t* kV4 = L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters";
        const wchar_t* kV6 = L"SYSTEM\\CurrentControlSet\\Services\\TCPIP6\\Parameters";
        x.probe = [kV4] { return reg::DwordEquals(HKLM, kV4, L"DefaultTTL", 65); };
        x.apply = [kV4, kV6](bool on) {
            if (on) {
                reg::SetDword(HKLM, kV4, L"DefaultTTL", 65);
                reg::SetDword(HKLM, kV6, L"DefaultTTL", 65);
            } else {
                // 128 is the Windows default, but it is not stored explicitly —
                // removing the override is what actually restores stock behavior.
                reg::DeleteValue(HKLM, kV4, L"DefaultTTL");
                reg::DeleteValue(HKLM, kV6, L"DefaultTTL");
            }
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "adv.indexing-off";
        x.page = "adv";
        x.locCategory = "adv";
        x.locSection = "main";
        x.locKey = "index_title";
        x.summary = "Disable the Windows Search indexer";
        x.needsAdmin = true;
        const wchar_t* kWSearch = L"SYSTEM\\CurrentControlSet\\Services\\WSearch";
        x.probe = [kWSearch] { return reg::DwordEquals(HKLM, kWSearch, L"Start", 4); };
        x.apply = [kWSearch](bool on) {
            reg::SetDword(HKLM, kWSearch, L"Start", on ? 4 : 2);
            proc::Run(L"sc", on ? L"stop WSearch" : L"start WSearch");
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "adv.legacy-boot-menu";
        x.page = "adv";
        x.locCategory = "adv";
        x.locSection = "main";
        x.locKey = "oldbootloader";
        x.summary = "Use the legacy text boot menu";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] {
            const std::string out = BcdEnum(L"{current}");
            return out.find("bootmenupolicy") != std::string::npos &&
                   out.find("legacy") != std::string::npos;
        };
        x.apply = [](bool on) {
            proc::Run(L"bcdedit", on ? L"/set {current} bootmenupolicy legacy"
                                     : L"/set {current} bootmenupolicy standard");
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "adv.advanced-boot-options";
        x.page = "adv";
        x.locCategory = "adv";
        x.locSection = "main";
        x.locKey = "advancedboot";
        x.summary = "Always show advanced boot options";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] { return BcdFlagEnabled(BcdEnum(L"{globalsettings}"), {"advancedoptions"}); };
        x.apply = [](bool on) {
            proc::Run(L"bcdedit", on ? L"/set {globalsettings} advancedoptions true"
                                     : L"/set {globalsettings} advancedoptions false");
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "adv.disable-pagefile";
        x.page = "adv";
        x.locCategory = "adv";
        x.locSection = "main";
        x.locKey = "swap";
        x.summary = "Disable the Windows page file";
        x.needsAdmin = true;
        x.reboot = Reboot::Restart;
        x.probe = [] {
            std::vector<std::wstring> files;
            if (!reg::GetMultiString(HKLM, kMemoryManagement, L"PagingFiles", files)) return true;
            for (const auto& f : files)
                if (f.find_first_not_of(L" \t") != std::wstring::npos) return false;
            return true;
        };
        x.apply = [](bool on) {
            const std::vector<std::wstring> value =
                on ? std::vector<std::wstring>{} : std::vector<std::wstring>{L"?:\\pagefile.sys"};
            reg::SetMultiString(HKLM, kMemoryManagement, L"PagingFiles", value);
        };
        t.push_back(std::move(x));
    }

    t.push_back(Action("adv.remove-edge", "adv", "adv", "deledge_btn",
                       "Uninstall Microsoft Edge and block its updater",
                       [] {
                           proc::RunHidden("cmd /c sc stop edgeupdate & sc delete edgeupdate & "
                                           "sc stop edgeupdatem & sc delete edgeupdatem");
                           proc::RunHidden(
                               "cmd /c schtasks /delete /tn \"MicrosoftEdgeUpdateTaskMachineCore\" "
                               "/f & schtasks /delete /tn \"MicrosoftEdgeUpdateTaskMachineUA\" /f");
                           reg::SetDword(HKLM, L"SOFTWARE\\Microsoft\\EdgeUpdate",
                                         L"DoNotUpdateToEdgeWithChromium", 1);
                           reg::SetDword(HKLM, L"SOFTWARE\\Policies\\Microsoft\\EdgeUpdate",
                                         L"InstallDefault", 0);
                           reg::SetDword(HKLM, L"SOFTWARE\\Policies\\Microsoft\\EdgeUpdate",
                                         L"UpdateDefault", 0);
                           proc::RunPowerShell(
                               L"Get-AppxPackage *Microsoft.MicrosoftEdge* | Remove-AppxPackage "
                               L"-ErrorAction SilentlyContinue");
                       },
                       Reboot::Restart));

    // ===================== Windows Components =====================

    t.push_back(Action("compon.directplay", "compon", "compon", "directplay",
                       "Enable the DirectPlay legacy component",
                       [] {
                           proc::Run(L"dism",
                                     L"/online /Enable-Feature /FeatureName:DirectPlay /All");
                       }));
    t.push_back(Action("compon.netfx3", "compon", "compon", "framework",
                       "Install .NET Framework 3.5",
                       [] { proc::RunPowerShell(L"Add-WindowsCapability -Online -Name NetFx3~~~~"); }));
    t.push_back(Action("compon.photo-viewer", "compon", "compon", "photoviewer",
                       "Restore Windows Photo Viewer file associations",
                       [] {
                           reg::SetString(HKEY_CLASSES_ROOT,
                                          L"Applications\\photoviewer.dll\\shell\\open", L"MuiVerb",
                                          L"@photoviewer.dll,-3043");
                           reg::SetString(
                               HKEY_CLASSES_ROOT,
                               L"Applications\\photoviewer.dll\\shell\\open\\command", L"",
                               L"%SystemRoot%\\System32\\rundll32.exe \"%ProgramFiles%\\Windows "
                               L"Photo Viewer\\PhotoViewer.dll\", ImageViewer_Fullscreen %1");
                           const wchar_t* assoc = L"SOFTWARE\\Microsoft\\Windows Photo Viewer"
                                                  L"\\Capabilities\\FileAssociations";
                           for (const wchar_t* ext :
                                {L".bmp", L".gif", L".jpeg", L".jpg", L".png", L".tif", L".tiff"})
                               reg::SetString(HKLM, assoc, ext, L"PhotoViewer.FileAssoc.Tiff");
                       }));
    t.push_back(Action("compon.gpedit", "compon", "compon", "gpedit",
                       "Install the Group Policy editor (RSAT tools)",
                       [] {
                           proc::RunPowerShell(
                               L"foreach ($i in 'gpedit','fde','gaudit','ppe') { dism /online "
                               L"/add-capability /CapabilityName:Rsat.$i.Tools~~~~0.0.1.0 }");
                       }));
    t.push_back(Action("compon.hyperv-off", "compon", "compon", "forcedis",
                       "Force-disable the Hyper-V hypervisor",
                       [] { proc::Run(L"bcdedit", L"/set hypervisorlaunchtype off"); },
                       Reboot::Restart));
    t.push_back(Action("compon.winsxs-cleanup", "compon", "compon", "winsxs",
                       "Compact the component store (WinSxS)",
                       [] {
                           proc::Run(L"dism",
                                     L"/online /Cleanup-Image /StartComponentCleanup /ResetBase");
                       }));
    t.push_back(Action("compon.powershell-remotesigned", "compon", "compon", "powershellscr",
                       "Set the PowerShell execution policy to RemoteSigned",
                       [] { proc::RunPowerShell(L"Set-ExecutionPolicy RemoteSigned -Force"); }));
    t.push_back(Action("compon.gamedvr-off", "compon", "compon", "xboxdvr",
                       "Disable Game DVR / background recording",
                       [] {
                           reg::SetDword(HKCU, L"System\\GameConfigStore", L"GameDVR_Enabled", 0);
                           reg::SetDword(HKLM,
                                         L"SOFTWARE\\Policies\\Microsoft\\Windows\\GameDVR",
                                         L"AllowGameDVR", 0);
                       }));

    // ===================== Performance =====================

    {
        Tweak x{};
        x.id = "perf.no-sleep-timeout";
        x.page = "perf";
        x.locCategory = "perfor";
        x.locSection = "main";
        x.locKey = "sleeptimeout";
        x.summary = "Never turn off the display or sleep";
        x.needsAdmin = true;
        x.probe = [] {
            return ReadAcIndex(kVideoSubgroup, kVideoPowerdownTimeout) == 0 &&
                   ReadAcIndex(kSleepSubgroup, kStandbyTimeout) == 0;
        };
        x.apply = [](bool on) {
            // Timeouts are seconds. The off values match the original's
            // powercfg -change defaults: 10/5 minutes display, 30/15 sleep.
            WriteIndexBoth(kVideoSubgroup, kVideoPowerdownTimeout, on ? 0 : 600, on ? 0 : 300);
            WriteIndexBoth(kSleepSubgroup, kStandbyTimeout, on ? 0 : 1800, on ? 0 : 900);
        };
        t.push_back(std::move(x));
    }

    {
        Tweak x{};
        x.id = "perf.ultimate-power-plan";
        x.page = "perf";
        x.locCategory = "perfor";
        x.locSection = "main";
        x.locKey = "ultperf";
        x.summary = "Activate the Ultimate Performance power plan";
        x.needsAdmin = true;
        x.probe = [] { return UltimatePerformanceActive(); };
        x.apply = [](bool on) {
            if (!on) {
                GUID balanced = kBalancedScheme;
                PowerSetActiveScheme(nullptr, &balanced);
                return;
            }
            GUID ultimate = EnsureUltimatePerformanceScheme();
            if (!IsZeroGuid(ultimate)) PowerSetActiveScheme(nullptr, &ultimate);
        };
        t.push_back(std::move(x));
    }

    return t;
}
} // namespace

const std::vector<Tweak>& All() {
    static const std::vector<Tweak> table = BuildTable();
    return table;
}

const Tweak* Find(std::string_view id) {
    for (const auto& t : All())
        if (id == t.id) return &t;
    return nullptr;
}

bool IsAvailable(const Tweak& t) {
    if (t.minBuild == 0 && t.maxBuild == 0) return true;
    const int build = os::GetWindowsBuild();
    if (t.minBuild != 0 && build < t.minBuild) return false;
    if (t.maxBuild != 0 && build > t.maxBuild) return false;
    return true;
}

std::vector<const Tweak*> ForPage(std::string_view page) {
    std::vector<const Tweak*> out;
    for (const auto& t : All())
        if (page == t.page && IsAvailable(t)) out.push_back(&t);
    return out;
}

std::string DisplayName(const Tweak& t, const l10n::Localization& l) {
    std::string name = l.Get(t.locCategory, t.locSection, t.locKey);
    // Localization::Get echoes the key back when it is missing.
    if (name == t.locKey || name.empty()) return t.summary;
    return name;
}

bool Probe(const Tweak& t) {
    if (!t.probe || !IsAvailable(t)) return false;
    return t.probe();
}

bool Apply(const Tweak& t, bool on) {
    if (!IsAvailable(t)) return false;
    if (t.kind == Kind::Action) {
        if (!t.action) return false;
        t.action();
        return true;
    }
    if (!t.apply) return false;
    t.apply(on);
    return true;
}

const std::vector<std::string>& TargetReleaseOptions() {
    static const std::vector<std::string> options = {"1607", "1709", "1809", "1909", "2004", "20H2",
                                                     "21H2", "22H2", "23H2", "24H2", "25H2", "26H2"};
    return options;
}

std::string GetTargetRelease() {
    std::wstring v;
    if (!reg::GetString(HKLM, kWindowsUpdatePolicy, L"TargetReleaseVersionInfo", v)) return {};
    return util::ToUtf8(v);
}

void SetTargetRelease(const std::string& release) {
    reg::SetDword(HKLM, kWindowsUpdatePolicy, L"TargetReleaseVersion", 1);
    reg::SetString(HKLM, kWindowsUpdatePolicy, L"TargetReleaseVersionInfo",
                   util::ToWide(release));
    // Windows 11 releases are gated on ProductVersion as well; without it the
    // pin silently does nothing on 22000+.
    reg::SetString(HKLM, kWindowsUpdatePolicy, L"ProductVersion",
                   os::GetWindowsBuild() >= 22000 ? L"Windows 11" : L"Windows 10");
}

std::wstring ActivePowerScheme() {
    const GUID active = ActiveSchemeGuid();
    if (IsZeroGuid(active)) return {};
    return GuidToString(active);
}

int GetCpuThrottleMax() { return ReadAcIndex(kProcessorSubgroup, kProcThrottleMax); }

bool SetCpuThrottleMax(int percent, std::string* error) {
    if (percent < 1 || percent > 100) {
        if (error) *error = "percent must be between 1 and 100";
        return false;
    }
    // Both AC and DC, otherwise the cap silently does nothing on battery.
    if (!WriteIndexBoth(kProcessorSubgroup, kProcThrottleMax, static_cast<DWORD>(percent),
                        static_cast<DWORD>(percent))) {
        if (error) *error = "the active power scheme rejected the new processor state limit";
        return false;
    }
    return true;
}

void ClearTargetRelease() {
    reg::DeleteValue(HKLM, kWindowsUpdatePolicy, L"TargetReleaseVersion");
    reg::DeleteValue(HKLM, kWindowsUpdatePolicy, L"TargetReleaseVersionInfo");
    reg::DeleteValue(HKLM, kWindowsUpdatePolicy, L"ProductVersion");
}

} // namespace maku::tweak
