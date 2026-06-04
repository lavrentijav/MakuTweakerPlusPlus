#pragma once
#include <string>
#include <tuple>

namespace maku::proc {

struct RunResult {
    int exitCode = -1;
    std::string output;
    std::string error;
};

RunResult Run(const std::wstring& file, const std::wstring& args, bool shell = false);
RunResult RunHidden(const std::string& commandLine);
RunResult RunPowerShell(const std::wstring& script);
void OpenUrl(const std::wstring& url);
void RestartExplorer();
void KillExplorer();

} // namespace maku::proc
