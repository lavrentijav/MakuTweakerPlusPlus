#include "platform/MetricsService.h"
#include "platform/MetricsTray.h"
#include "core/LogFile.h"
#include "core/MetricsDb.h"
#include "core/MetricsSampler.h"
#include "core/MetricsWriteBuffer.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include <algorithm>
#include <mutex>
#include <windows.h>
#include <wtsapi32.h>

namespace maku::metrics_svc {
namespace {

constexpr DWORD kSettingsRefreshMs = 10000; // re-read sample interval from disk
constexpr DWORD kTrayHealthCheckMs = 30000;
constexpr DWORD kVacuumPeriodMs = 24 * 60 * 60 * 1000; // 24h

SERVICE_STATUS g_status{};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
HANDLE g_stopEvent = nullptr;

struct CachedScm {
    DWORD lastCheckTick = 0;
    bool installed = false;
    bool running = false;
};
CachedScm g_scmCache{};
std::mutex g_scmMutex;

void ReportSvcStatus(DWORD state, DWORD exitCode = NO_ERROR, DWORD waitHint = 0) {
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = exitCode;
    g_status.dwWaitHint = waitHint;
    if (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING)
        g_status.dwControlsAccepted = 0;
    else
        g_status.dwControlsAccepted =
            SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_SESSIONCHANGE;
    SetServiceStatus(g_statusHandle, &g_status);
}

DWORD WINAPI ServiceCtrlHandlerEx(DWORD ctrl, DWORD eventType, LPVOID /*eventData*/,
                                  LPVOID /*context*/) {
    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 5000);
        if (g_stopEvent) SetEvent(g_stopEvent);
        return NO_ERROR;
    case SERVICE_CONTROL_SESSIONCHANGE:
        if (eventType == WTS_SESSION_LOGON || eventType == WTS_CONSOLE_CONNECT) {
            metrics_tray::LaunchInUserSession();
        }
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        SetServiceStatus(g_statusHandle, &g_status);
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

int LoadSampleIntervalSec() {
    Settings s;
    s.Load();
    return std::clamp(s.metricsIntervalSec, kMetricsMinIntervalSec, kMetricsMaxIntervalSec);
}

VOID WINAPI ServiceMain(DWORD, LPWSTR*) {
    g_statusHandle = RegisterServiceCtrlHandlerExW(kServiceName, ServiceCtrlHandlerEx, nullptr);
    if (!g_statusHandle) return;

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwServiceSpecificExitCode = 0;
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 5000);

    SetCurrentDirectoryW(util::GetExeDirectory().c_str());
    logf::InitDefault();
    logf::Info("Metrics service ServiceMain start");

    g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!g_stopEvent) {
        const DWORD err = GetLastError();
        logf::Error("Metrics service CreateEvent failed: " + std::to_string(err));
        ReportSvcStatus(SERVICE_STOPPED, err);
        return;
    }

    metrics::MetricsSampler sampler;
    metrics::MetricsDb db;
    metrics::MetricsWriteBuffer buffer;
    const bool samplerOk = sampler.InitForService();
    const bool dbOk = db.Open(db.DefaultPath(), true);
    buffer.Reset(GetTickCount());

    if (!samplerOk) logf::Error("Metrics service: sampler init failed");
    if (!dbOk) logf::Error("Metrics service: DB open failed");

    ReportSvcStatus(SERVICE_RUNNING);
    metrics_tray::LaunchInUserSession();

    int intervalSec = LoadSampleIntervalSec();
    DWORD nextSampleTick = GetTickCount();
    DWORD lastSettingsTick = GetTickCount();
    DWORD lastTrayTick = GetTickCount();
    DWORD lastVacuumTick = GetTickCount();
    DWORD lastFlushTick = GetTickCount();

    while (WaitForSingleObject(g_stopEvent, kMetricsServicePollMs) == WAIT_TIMEOUT) {
        const DWORD now = GetTickCount();

        if (now - lastSettingsTick >= kSettingsRefreshMs) {
            lastSettingsTick = now;
            intervalSec = LoadSampleIntervalSec();
        }

        if (now - lastTrayTick >= kTrayHealthCheckMs) {
            lastTrayTick = now;
            metrics_tray::EnsureStandaloneTray();
        }

        if (samplerOk && dbOk && static_cast<int32_t>(now - nextSampleTick) >= 0) {
            metrics::LiveSample sample;
            if (sampler.Sample(sample)) {
                buffer.PushSystem(sample.system);
                if (!sample.perLogical.empty())
                    buffer.PushCpu(sample.system.ts, sample.perLogical);
            }
            nextSampleTick = now + static_cast<DWORD>(intervalSec) * 1000u;
        }

        if (buffer.IsFull() || buffer.ShouldFlush(now)) {
            buffer.Flush(db);
            lastFlushTick = now;
        }

        if (dbOk && now - lastVacuumTick >= kVacuumPeriodMs) {
            lastVacuumTick = now;
            db.RunMaintenance(/*force=*/true);
        }

        (void)lastFlushTick;
    }

    logf::Info("Metrics service stop requested, flushing buffers");
    buffer.Flush(db);

    CloseHandle(g_stopEvent);
    g_stopEvent = nullptr;
    db.Close();
    sampler.Shutdown();
    logf::Info("Metrics service ServiceMain exit");
    ReportSvcStatus(SERVICE_STOPPED);
}

SC_HANDLE OpenScm(DWORD access) { return OpenSCManagerW(nullptr, nullptr, access); }

SC_HANDLE OpenSvc(DWORD access) {
    SC_HANDLE scm = OpenScm(SC_MANAGER_CONNECT);
    if (!scm) return nullptr;
    SC_HANDLE svc = OpenServiceW(scm, kServiceName, access);
    CloseServiceHandle(scm);
    return svc;
}

void ApplyServiceMetadata(SC_HANDLE svc) {
    SERVICE_DESCRIPTIONW desc{};
    desc.lpDescription = const_cast<wchar_t*>(kServiceDescription);
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

    SERVICE_DELAYED_AUTO_START_INFO delayed{};
    delayed.fDelayedAutostart = TRUE;
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &delayed);

    SERVICE_FAILURE_ACTIONSW fa{};
    SC_ACTION actions[3]{};
    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 30000;
    actions[1].Type = SC_ACTION_RESTART;
    actions[1].Delay = 60000;
    actions[2].Type = SC_ACTION_NONE;
    actions[2].Delay = 0;
    fa.dwResetPeriod = 24 * 60 * 60;
    fa.cActions = 3;
    fa.lpsaActions = actions;
    ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);
}

bool UpdateServiceConfig(SC_HANDLE svc, const std::wstring& cmdLine) {
    const bool ok = ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_AUTO_START,
                                         SERVICE_ERROR_NORMAL, cmdLine.c_str(), nullptr, nullptr,
                                         nullptr, nullptr, nullptr,
                                         kServiceDisplayName) != FALSE;
    if (ok) ApplyServiceMetadata(svc);
    return ok;
}

bool QueryScmFresh(bool& installedOut, bool& runningOut) {
    installedOut = false;
    runningOut = false;
    SC_HANDLE svc = OpenSvc(SERVICE_QUERY_STATUS);
    if (!svc) return false;
    installedOut = true;
    SERVICE_STATUS st{};
    if (QueryServiceStatus(svc, &st))
        runningOut = (st.dwCurrentState == SERVICE_RUNNING);
    CloseServiceHandle(svc);
    return true;
}

} // namespace

std::wstring BuildServiceCommandLine() {
    return L"\"" + util::GetExePath() + L"\"" + kServiceLaunchArgs;
}

bool IsInstalled() {
    SC_HANDLE svc = OpenSvc(SERVICE_QUERY_STATUS);
    if (!svc) return false;
    CloseServiceHandle(svc);
    return true;
}

bool IsRunning() {
    SC_HANDLE svc = OpenSvc(SERVICE_QUERY_STATUS);
    if (!svc) return false;
    SERVICE_STATUS st{};
    const bool ok = QueryServiceStatus(svc, &st) && st.dwCurrentState == SERVICE_RUNNING;
    CloseServiceHandle(svc);
    return ok;
}

namespace {
void RefreshScmCacheIfStale() {
    const DWORD now = GetTickCount();
    std::lock_guard<std::mutex> lock(g_scmMutex);
    if (g_scmCache.lastCheckTick != 0 && now - g_scmCache.lastCheckTick < 1000) return;
    bool installed = false, running = false;
    if (QueryScmFresh(installed, running)) {
        g_scmCache.installed = installed;
        g_scmCache.running = running;
    } else {
        g_scmCache.installed = false;
        g_scmCache.running = false;
    }
    g_scmCache.lastCheckTick = now ? now : 1;
}
} // namespace

bool IsInstalledCached() {
    RefreshScmCacheIfStale();
    std::lock_guard<std::mutex> lock(g_scmMutex);
    return g_scmCache.installed;
}

bool IsRunningCached() {
    RefreshScmCacheIfStale();
    std::lock_guard<std::mutex> lock(g_scmMutex);
    return g_scmCache.running;
}

bool GetStatus(wchar_t* buf, DWORD bufChars) {
    if (!buf || bufChars == 0) return false;
    if (!IsInstalledCached()) {
        swprintf_s(buf, bufChars, L"Not installed");
        return true;
    }
    if (IsRunningCached()) {
        swprintf_s(buf, bufChars, L"Running");
        return true;
    }
    swprintf_s(buf, bufChars, L"Stopped");
    return true;
}

bool Install() {
    const std::wstring cmdLine = BuildServiceCommandLine();

    SC_HANDLE existing = OpenSvc(SERVICE_CHANGE_CONFIG | SERVICE_START | SERVICE_QUERY_STATUS);
    if (existing) {
        const bool updated = UpdateServiceConfig(existing, cmdLine);
        CloseServiceHandle(existing);
        if (!updated) return false;
        // Invalidate cache so callers see fresh state after install.
        std::lock_guard<std::mutex> lock(g_scmMutex);
        g_scmCache.lastCheckTick = 0;
        return Start();
    }

    SC_HANDLE scm = OpenScm(SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        logf::Error("Install metrics service: OpenSCManager failed " + std::to_string(GetLastError()));
        return false;
    }

    SC_HANDLE svc = CreateServiceW(scm, kServiceName, kServiceDisplayName, SERVICE_ALL_ACCESS,
                                   SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                                   SERVICE_ERROR_NORMAL, cmdLine.c_str(), nullptr, nullptr, nullptr,
                                   nullptr, nullptr);
    const DWORD createErr = svc ? ERROR_SUCCESS : GetLastError();
    CloseServiceHandle(scm);
    if (!svc) {
        logf::Error("CreateService failed: " + std::to_string(createErr));
        return false;
    }
    ApplyServiceMetadata(svc);
    CloseServiceHandle(svc);
    {
        std::lock_guard<std::mutex> lock(g_scmMutex);
        g_scmCache.lastCheckTick = 0;
    }
    return Start();
}

bool InstallElevated() {
    if (os::IsElevated()) return Install();

    const auto r = proc::RunElevated(util::GetExePath(), L"--install-metrics-service");
    if (r.exitCode == -1) return false;
    return IsInstalled();
}

bool Uninstall() {
    Stop();
    SC_HANDLE scm = OpenScm(SC_MANAGER_CONNECT);
    if (!scm) return false;
    SC_HANDLE svc = OpenServiceW(scm, kServiceName, DELETE);
    CloseServiceHandle(scm);
    if (!svc) return false;
    const bool ok = DeleteService(svc);
    CloseServiceHandle(svc);
    {
        std::lock_guard<std::mutex> lock(g_scmMutex);
        g_scmCache.lastCheckTick = 0;
    }
    return ok;
}

bool Start() {
    SC_HANDLE svc = OpenSvc(SERVICE_START | SERVICE_QUERY_STATUS);
    if (!svc) return false;
    bool ok = StartServiceW(svc, 0, nullptr) != FALSE;
    if (!ok && GetLastError() == ERROR_SERVICE_ALREADY_RUNNING) ok = true;
    // Wait briefly for RUNNING state so callers (UI) see consistent state.
    if (ok) {
        SERVICE_STATUS st{};
        for (int i = 0; i < 20; ++i) {
            if (!QueryServiceStatus(svc, &st)) break;
            if (st.dwCurrentState == SERVICE_RUNNING) break;
            Sleep(100);
        }
    }
    CloseServiceHandle(svc);
    {
        std::lock_guard<std::mutex> lock(g_scmMutex);
        g_scmCache.lastCheckTick = 0;
    }
    return ok;
}

bool EnsureAutostartAndRunning() {
    if (!IsInstalled()) {
        if (!Install() && !InstallElevated()) return false;
    } else {
        SC_HANDLE svc = OpenSvc(SERVICE_CHANGE_CONFIG);
        if (svc) {
            UpdateServiceConfig(svc, BuildServiceCommandLine());
            CloseServiceHandle(svc);
        }
    }
    return Start();
}

bool Stop() {
    SC_HANDLE svc = OpenSvc(SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (!svc) return false;
    SERVICE_STATUS st{};
    const bool ok = ControlService(svc, SERVICE_CONTROL_STOP, &st) != FALSE;
    if (ok) {
        for (int i = 0; i < 30; ++i) {
            if (!QueryServiceStatus(svc, &st)) break;
            if (st.dwCurrentState == SERVICE_STOPPED) break;
            Sleep(100);
        }
    }
    CloseServiceHandle(svc);
    {
        std::lock_guard<std::mutex> lock(g_scmMutex);
        g_scmCache.lastCheckTick = 0;
    }
    return ok;
}

int RunServiceMain() {
    SERVICE_TABLE_ENTRYW table[] = {{const_cast<LPWSTR>(kServiceName),
                                     const_cast<LPSERVICE_MAIN_FUNCTIONW>(ServiceMain)},
                                    {nullptr, nullptr}};
    if (!StartServiceCtrlDispatcherW(table)) return static_cast<int>(GetLastError());
    return 0;
}

bool CollectOnce(int intervalSec) {
    (void)intervalSec;
    metrics::MetricsSampler sampler;
    metrics::MetricsDb db;
    metrics::MetricsWriteBuffer buffer;
    if (!sampler.InitForService()) return false;
    if (!db.Open(db.DefaultPath(), true)) return false;
    buffer.Reset(GetTickCount());
    metrics::LiveSample sample;
    if (!sampler.Sample(sample)) return false;
    buffer.PushSystem(sample.system);
    if (!sample.perLogical.empty()) buffer.PushCpu(sample.system.ts, sample.perLogical);
    buffer.Flush(db);
    return true;
}

} // namespace maku::metrics_svc
