#include "core/ProcessMgrUtil.h"
#include "core/StringUtil.h"
#include <psapi.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cwctype>

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
              [](const ProcRow& a, const ProcRow& b) { return a.mem > b.mem; });
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

} // namespace maku::pmgr
