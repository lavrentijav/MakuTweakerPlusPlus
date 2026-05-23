#pragma once
#include <functional>
#include <string>
#include <tuple>

namespace maku::proc {

struct RunResult {
    int exitCode = -1;
    std::string output;
    std::string error;
};

RunResult Run(const std::wstring& file, const std::wstring& args, bool shell = false);
/// Separate UAC per process (CLI / scheduled tasks only — not used from the GUI).
RunResult RunElevated(const std::wstring& file, const std::wstring& args);
RunResult RunHidden(const std::string& commandLine);
RunResult RunHiddenElevated(const std::string& commandLine);
RunResult RunPowerShell(const std::wstring& script);
RunResult RunPowerShellElevated(const std::wstring& script);
/// Requires elevation (GUI relaunch via os::EnsureAdmin).
RunResult RunPowerShellAdmin(const std::wstring& script);
using OutputLineFn = std::function<void(const std::string& line)>;
RunResult RunPowerShellAdminStreaming(const std::wstring& script, OutputLineFn onLine);
/// Runs only when the app is already elevated (GUI relaunches once via os::EnsureAdmin).
RunResult RunAdmin(const std::wstring& file, const std::wstring& args);
RunResult RunHiddenAdmin(const std::string& commandLine);
void OpenUrl(const std::wstring& url);
void KillExplorer();
void StartExplorer();
void RestartExplorer();

} // namespace maku::proc
