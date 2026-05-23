#include "core/ProcessMgrUtil.h"
#include "core/StringUtil.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>
#include <unordered_set>

namespace maku::pmgr {
namespace {

using NtQueryInformationProcessFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
constexpr ULONG kProcessBreakOnTermination = 29;

bool QueryProcessCritical(HANDLE h) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto fn = reinterpret_cast<NtQueryInformationProcessFn>(
        GetProcAddress(ntdll, "NtQueryInformationProcess"));
    if (!fn) return false;
    ULONG critical = 0;
    if (fn(h, kProcessBreakOnTermination, &critical, sizeof(critical), nullptr) != 0)
        return false;
    return critical != 0;
}

std::wstring QueryProcessImagePath(HANDLE h) {
    wchar_t buf[MAX_PATH]{};
    DWORD size = MAX_PATH;
    if (QueryFullProcessImageNameW(h, 0, buf, &size)) return buf;
    return {};
}

bool IsProcessFrozen(DWORD pid) {
    struct Ctx {
        DWORD pid;
        HWND hwnd = nullptr;
    } ctx{pid};

    EnumWindows(
        [](HWND hwnd, LPARAM lp) -> BOOL {
            auto* c = reinterpret_cast<Ctx*>(lp);
            DWORD wpid = 0;
            GetWindowThreadProcessId(hwnd, &wpid);
            if (wpid == c->pid && IsWindowVisible(hwnd)) {
                c->hwnd = hwnd;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&ctx));

    return ctx.hwnd && IsHungAppWindow(ctx.hwnd);
}

std::wstring ToLower(std::wstring s) {
    for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
    return s;
}

std::wstring NormalizeProcKey(std::wstring name) {
    name = ToLower(std::move(name));
    if (name.size() > 4 && name.compare(name.size() - 4, 4, L".exe") == 0)
        name.resize(name.size() - 4);
    return name;
}

const std::unordered_set<std::wstring>& HiddenProcessNames() {
    static const std::unordered_set<std::wstring> names = [] {
        static const wchar_t* kNames[] = {
            L"dwm", L"msedgewebview2", L"startmenuexperiencehost", L"taskmgr", L"explorer",
            L"system", L"idle", L"dllhost", L"smss", L"csrss", L"wininit", L"services", L"lsass",
            L"winlogon", L"svchost", L"fontdrvhost", L"sihost", L"shellexperiencehost", L"ctfmon",
            L"runtimebroker", L"searchindexer", L"msbuild", L"crossdeviceservice",
            L"bioenrollmenthost", L"acergaicameraw", L"vmtoolsd", L"searchapp", L"wpfsurface",
            L"searchhost", L"phoneexperiencehost", L"textinputhost", L"nvidia overlay",
            L"vscodium", L"lockapp", L"shellhost", L"systemsettings", L"crossdeviceresume",
            L"applicationframehost", L"searchui", L"gamebar", L"xboxgamebarwidgets",
            L"xboxpcappft", L"icloudservices", L"nvdisplay.container", L"widgets",
            L"xboxgamebarspotify", L"backgroundtaskhost", L"perfwatson2", L"onedrive",
            L"onedrive.sync.service", L"igcctray", L"igcc", L"microsoft.cmdpal.ui", L"wwahost",
            L"onedrive.setup", L"rtkuwp", L"makutweaker", L"msedge", L"nvcontainer",
            L"snippingtool", L"softlandingtask", L"unsecapp", L"gameinputredistservice",
            L"accuserps", L"useroobebroker", L"smartscreen", L"nvsphelper64",
            L"acerhardwareservice", L"rtkauduservice64", L"acersyshardwareservice", L"openrgb",
            L"widgetservice", L"applemobiledeviceprocess", L"aqauserps",
            L"windowspackagemanagerserver", L"dataexchangehost", L"inputpersonalization",
            L"bootcamp", L"settingsynchost", L"igfxtray", L"igfxhk", L"securityhealthsystray",
            L"filecoauth", L"storedesktopextension", L"vm3dservice", L"rundll32",
            L"searchprotocolhost", L"backgroundtransferhost", L"xgamehelper", L"comppkgsrv",
            L"onedrivestandaloneupdater", L"gamebarftserver", L"appactions",
            L"systemsettingsbroker",
        };
        std::unordered_set<std::wstring> set;
        for (const wchar_t* n : kNames) set.insert(NormalizeProcKey(n));
        return set;
    }();
    return names;
}

} // namespace

bool MatchesSearch(const std::wstring& haystack, const std::wstring& query) {
    if (query.empty()) return true;
    const std::wstring h = ToLower(haystack);
    const std::wstring q = ToLower(query);
    return h.find(q) != std::wstring::npos;
}

bool IsExcluded(const std::wstring& exeName, const std::string& exclusionsCsv) {
    if (exclusionsCsv.empty()) return false;
    const std::wstring nameLower = ToLower(exeName);
    const std::wstring csv = util::ToWide(exclusionsCsv);
    std::wstring token;
    for (wchar_t ch : csv) {
        if (ch == L',' || ch == L';') {
            if (!token.empty()) {
                while (!token.empty() && iswspace(token.front())) token.erase(token.begin());
                while (!token.empty() && iswspace(token.back())) token.pop_back();
                if (ToLower(token) == nameLower) return true;
                token.clear();
            }
            continue;
        }
        token.push_back(ch);
    }
    if (!token.empty()) {
        while (!token.empty() && iswspace(token.front())) token.erase(token.begin());
        while (!token.empty() && iswspace(token.back())) token.pop_back();
        if (ToLower(token) == nameLower) return true;
    }
    return false;
}

bool IsSystemProcess(const ProcRow& row) {
    if (row.pid <= 4) return true;
    if (row.sessionId == 0) return true;
    return HiddenProcessNames().count(NormalizeProcKey(row.name)) != 0;
}

bool IsSystemService(const SvcRow& row) { return row.critical; }

void RefreshProcesses(std::vector<ProcRow>& out) {
    out.clear();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(snap, &pe)) {
        do {
            ProcRow row;
            row.pid = pe.th32ProcessID;
            row.name = pe.szExeFile;
            ProcessIdToSessionId(pe.th32ProcessID, &row.sessionId);

            DWORD access = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ;
            HANDLE h = OpenProcess(access, FALSE, pe.th32ProcessID);
            if (h) {
                PROCESS_MEMORY_COUNTERS pmc{};
                if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
                    row.mem = pmc.WorkingSetSize;
                row.path = QueryProcessImagePath(h);
                row.critical = QueryProcessCritical(h);
                CloseHandle(h);
            }
            row.frozen = IsProcessFrozen(pe.th32ProcessID);
            out.push_back(std::move(row));
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);

    std::sort(out.begin(), out.end(),
              [](const ProcRow& a, const ProcRow& b) {
                  const int cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
                  return cmp != 0 ? cmp < 0 : a.pid < b.pid;
              });
}

void RefreshServices(std::vector<SvcRow>& out) {
    out.clear();
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scm) return;

    DWORD bytesNeeded = 0;
    DWORD servicesReturned = 0;
    DWORD resume = 0;
    EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32,
                          SERVICE_STATE_ALL, nullptr, 0, &bytesNeeded,
                          &servicesReturned, &resume, nullptr);

    std::vector<BYTE> buffer(bytesNeeded);
    if (!EnumServicesStatusExW(
            scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
            buffer.data(), static_cast<DWORD>(buffer.size()), &bytesNeeded,
            &servicesReturned, &resume, nullptr)) {
        CloseServiceHandle(scm);
        return;
    }

    auto* entries = reinterpret_cast<ENUM_SERVICE_STATUS_PROCESSW*>(buffer.data());
    for (DWORD i = 0; i < servicesReturned; ++i) {
        const auto& e = entries[i];
        SvcRow row;
        row.name = e.lpServiceName;
        row.displayName = e.lpDisplayName ? e.lpDisplayName : e.lpServiceName;
        row.state = e.ServiceStatusProcess.dwCurrentState;

        SC_HANDLE svc =
            OpenServiceW(scm, e.lpServiceName, SERVICE_QUERY_CONFIG);
        if (svc) {
            DWORD needed = 0;
            QueryServiceConfigW(svc, nullptr, 0, &needed);
            std::vector<BYTE> cfgBuf(needed);
            auto* cfg = reinterpret_cast<QUERY_SERVICE_CONFIGW*>(cfgBuf.data());
            if (QueryServiceConfigW(svc, cfg, needed, &needed))
                row.startType = cfg->dwStartType;
            CloseServiceHandle(svc);
        }

        row.critical = row.startType == SERVICE_BOOT_START ||
                       row.startType == SERVICE_SYSTEM_START;
        out.push_back(std::move(row));
    }
    CloseServiceHandle(scm);

    std::sort(out.begin(), out.end(),
              [](const SvcRow& a, const SvcRow& b) {
                  return _wcsicmp(a.name.c_str(), b.name.c_str()) < 0;
              });
}

std::wstring ServiceStateText(DWORD state) {
    switch (state) {
    case SERVICE_RUNNING: return L"Running";
    case SERVICE_STOPPED: return L"Stopped";
    case SERVICE_PAUSED: return L"Paused";
    case SERVICE_START_PENDING: return L"Start pending";
    case SERVICE_STOP_PENDING: return L"Stop pending";
    case SERVICE_CONTINUE_PENDING: return L"Continue pending";
    case SERVICE_PAUSE_PENDING: return L"Pause pending";
    default: return L"Unknown";
    }
}

std::wstring ServiceStartTypeText(DWORD startType) {
    switch (startType) {
    case SERVICE_AUTO_START: return L"Automatic";
    case SERVICE_DEMAND_START: return L"Manual";
    case SERVICE_DISABLED: return L"Disabled";
    case SERVICE_BOOT_START: return L"Boot";
    case SERVICE_SYSTEM_START: return L"System";
    default: return L"Unknown";
    }
}

bool KillProcess(const DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return false;
    const BOOL ok = TerminateProcess(h, 0);
    CloseHandle(h);
    return ok != FALSE;
}

void KillProcessesByImagePath(const std::wstring& path) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(snap, &pe)) {
        do {
            HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE, FALSE,
                                   pe.th32ProcessID);
            if (!h) continue;
            wchar_t buf[MAX_PATH]{};
            DWORD sz = MAX_PATH;
            if (QueryFullProcessImageNameW(h, 0, buf, &sz) &&
                _wcsicmp(buf, path.c_str()) == 0)
                TerminateProcess(h, 0);
            CloseHandle(h);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

void KillProcessesByName(const std::wstring& exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;

    PROCESSENTRY32W pe{sizeof(pe)};
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exeName.c_str()) == 0)
                KillProcess(pe.th32ProcessID);
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
}

bool StopService(const std::wstring& name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    SERVICE_STATUS status{};
    const BOOL ok = ControlService(svc, SERVICE_CONTROL_STOP, &status);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok != FALSE;
}

bool SetServiceStartType(const std::wstring& name, const DWORD startType) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;

    SC_HANDLE svc = OpenServiceW(scm, name.c_str(), SERVICE_CHANGE_CONFIG);
    if (!svc) {
        CloseServiceHandle(scm);
        return false;
    }

    const BOOL ok =
        ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, startType, SERVICE_NO_CHANGE, nullptr,
                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok != FALSE;
}

} // namespace maku::pmgr
