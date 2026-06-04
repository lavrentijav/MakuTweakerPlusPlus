#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <windows.h>
#include <shellapi.h>
#include <sstream>

namespace maku::proc {

static RunResult ReadProcess(HANDLE hProcess, HANDLE hStdout, HANDLE hStderr) {
    RunResult r;
    char buf[4096];
    DWORD read{};
    for (;;) {
        if (!ReadFile(hStdout, buf, sizeof(buf) - 1, &read, nullptr) || read == 0) break;
        buf[read] = 0;
        r.output += buf;
    }
    for (;;) {
        if (!ReadFile(hStderr, buf, sizeof(buf) - 1, &read, nullptr) || read == 0) break;
        buf[read] = 0;
        r.error += buf;
    }
    WaitForSingleObject(hProcess, INFINITE);
    DWORD code{};
    GetExitCodeProcess(hProcess, &code);
    r.exitCode = static_cast<int>(code);
    return r;
}

RunResult Run(const std::wstring& file, const std::wstring& args, bool shell) {
    RunResult r;
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE outRd{}, outWr{}, errRd{}, errWr{};
    CreatePipe(&outRd, &outWr, &sa, 0);
    CreatePipe(&errRd, &errWr, &sa, 0);
    SetHandleInformation(outRd, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(errRd, HANDLE_FLAG_INHERIT, 0);

    std::wstring cmd = L"\"" + file + L"\" " + args;
    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdOutput = outWr;
    si.hStdError = errWr;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    DWORD flags = CREATE_NO_WINDOW;
    if (shell) flags |= CREATE_UNICODE_ENVIRONMENT;

    if (!CreateProcessW(shell ? nullptr : file.c_str(), cmdBuf.data(), nullptr, nullptr, TRUE,
                        flags, nullptr, nullptr, &si, &pi)) {
        CloseHandle(outRd);
        CloseHandle(errRd);
        return r;
    }
    CloseHandle(outWr);
    CloseHandle(errWr);
    r = ReadProcess(pi.hProcess, outRd, errRd);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(outRd);
    CloseHandle(errRd);
    return r;
}

RunResult RunHidden(const std::string& commandLine) {
    return Run(L"cmd.exe", L"/C " + maku::util::ToWide(commandLine));
}

RunResult RunPowerShell(const std::wstring& script) {
    return Run(L"powershell.exe",
               L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command " + script);
}

void OpenUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void KillExplorer() {
    Run(L"taskkill", L"/F /IM explorer.exe");
}

void RestartExplorer() {
    KillExplorer();
    Sleep(2000);
    Run(L"explorer.exe", L"");
}

} // namespace maku::proc
