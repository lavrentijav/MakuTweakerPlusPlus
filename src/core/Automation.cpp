#include "core/Automation.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"

namespace maku::auto_task {
namespace {

std::wstring QuotedExe() { return L"\"" + util::GetExePath() + L"\""; }

bool SchTaskExists(const wchar_t* name) {
    const auto r = proc::Run(L"schtasks", std::wstring(L"/Query /TN ") + name);
    return r.exitCode == 0;
}

bool CreateTask(const wchar_t* name, const std::wstring& args, const wchar_t* scheduleExtra) {
    proc::Run(L"schtasks", std::wstring(L"/Delete /TN ") + name + L" /F");
    std::wstring cmd = L"/Create /TN " + std::wstring(name) + L" /TR " + QuotedExe() + L" " + args +
                       L" /SC " + scheduleExtra + L" /RL HIGHEST /F";
    return proc::Run(L"schtasks", cmd).exitCode == 0;
}

} // namespace

bool InstallBootTtl() {
    return CreateTask(L"MakuTweakerPlusPlus_TTL", L"--ttl-bypass", L"ONLOGON");
}

bool RemoveBootTtl() {
    return proc::Run(L"schtasks", L"/Delete /TN MakuTweakerPlusPlus_TTL /F").exitCode == 0;
}

bool InstallWeeklyCleanup() {
    return CreateTask(L"MakuTweakerPlusPlus_Cleanup", L"--cleanup-temp", L"WEEKLY /D SUN /ST 03:00");
}

bool RemoveWeeklyCleanup() {
    return proc::Run(L"schtasks", L"/Delete /TN MakuTweakerPlusPlus_Cleanup /F").exitCode == 0;
}

bool IsBootTtlInstalled() { return SchTaskExists(L"MakuTweakerPlusPlus_TTL"); }
bool IsWeeklyCleanupInstalled() { return SchTaskExists(L"MakuTweakerPlusPlus_Cleanup"); }

void RunTempCleanup() {
    proc::RunHidden("del /q /f /s %TEMP%\\* 2>nul");
    proc::RunHidden("del /q /f /s %LOCALAPPDATA%\\Temp\\* 2>nul");
    proc::RunHidden("for /d %i in (%TEMP%\\*) do @rd /s /q \"%i\" 2>nul");
}

} // namespace maku::auto_task
