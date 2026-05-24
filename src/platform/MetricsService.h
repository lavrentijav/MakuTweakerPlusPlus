#pragma once
#include <windows.h>
#include <string>

namespace maku::metrics_svc {

constexpr wchar_t kServiceName[] = L"MakuTweakerMetrics";
constexpr wchar_t kServiceDisplayName[] = L"MakuTweaker++ Metrics";
constexpr wchar_t kServiceDescription[] =
    L"Background collector for CPU/RAM/GPU/disk/network samples used by MakuTweaker++ Monitor. "
    L"Persists rolling history to %ProgramData%\\MakuTweaker++\\metrics.db.";
/// Appended to the installed exe path in SCM (same binary, service entry mode).
constexpr wchar_t kServiceLaunchArgs[] = L" --metrics-service";

/// Service poll cadence used by the GUI when the service is running. The service itself
/// samples on a separate schedule driven by user settings (see RecommendedIntervalSec()).
constexpr DWORD kMetricsServicePollMs = 1000;

/// User-tunable sampling interval bounds (seconds).
constexpr int kMetricsMinIntervalSec = 1;
constexpr int kMetricsMaxIntervalSec = 60;

std::wstring BuildServiceCommandLine();

bool IsInstalled();
bool IsRunning();
bool GetStatus(wchar_t* buf, DWORD bufChars);

/// Lightweight cached query (refreshed every ~1s). Use from UI hot paths.
bool IsInstalledCached();
bool IsRunningCached();

bool Install();
bool InstallElevated();
bool Uninstall();
bool Start();
bool Stop();

/// Install if needed, ensure SERVICE_AUTO_START, and start the service.
bool EnsureAutostartAndRunning();

/// Run service main loop (blocks until service stops). Returns process exit code.
int RunServiceMain();

/// Single collection tick (used by service and headless CLI).
bool CollectOnce(int intervalSec);

} // namespace maku::metrics_svc
