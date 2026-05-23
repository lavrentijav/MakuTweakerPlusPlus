#include "core/ProcessRunner.h"
#include "core/LogFile.h"
#include "core/OsUtil.h"
#include "core/StringUtil.h"
#include <functional>
#include <windows.h>
#include <wincrypt.h>
#include <shellapi.h>
#include <sstream>
#include <vector>

namespace maku::proc {
namespace {

std::wstring PowerShellExePath() {
    wchar_t sysDir[MAX_PATH]{};
    if (GetSystemDirectoryW(sysDir, MAX_PATH) == 0) return L"powershell.exe";
    std::wstring path = std::wstring(sysDir) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
    return L"powershell.exe";
}

std::wstring WriteTempScript(const std::wstring& script) {
    wchar_t tempDir[MAX_PATH]{};
    if (GetTempPathW(MAX_PATH, tempDir) == 0) return {};
    const std::wstring path =
        std::wstring(tempDir) + L"maku_" + std::to_wstring(GetTickCount64()) + L".ps1";
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    DWORD written = 0;
    const char bom[] = "\xEF\xBB\xBF";
    const std::string utf8 = util::ToUtf8(script);
    WriteFile(h, bom, 3, &written, nullptr);
    WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(h);
    return path;
}

void LogProcess(const char* tag, const std::wstring& file, const std::wstring& args,
                const RunResult& r) {
    if (!logf::Active()) return;
    std::string msg = std::string(tag) + " " + util::ToUtf8(file);
    if (!args.empty()) msg += " " + util::ToUtf8(args);
    msg += " exit=" + std::to_string(r.exitCode);
    logf::Info(msg);
    if (!r.error.empty()) logf::Warn("stderr: " + r.error);
    if (!r.output.empty() && r.output.size() < 4096) logf::Info("stdout: " + r.output);
}

} // namespace

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
        if (logf::Active()) {
            logf::Error("CreateProcess failed (" + util::ToUtf8(file) +
                        "): Win32=" + std::to_string(GetLastError()));
        }
        CloseHandle(outWr);
        CloseHandle(errWr);
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
    LogProcess("run", file, args, r);
    return r;
}

static void EmitOutputLines(std::string& pending, const char* chunk, size_t len,
                            OutputLineFn& onLine, std::string& fullOut) {
    if (len == 0) return;
    fullOut.append(chunk, len);
    pending.append(chunk, len);
    for (;;) {
        const size_t pos = pending.find('\n');
        if (pos == std::string::npos) break;
        std::string line = pending.substr(0, pos);
        pending.erase(0, pos + 1);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (onLine && !line.empty()) onLine(line);
    }
}

static RunResult RunStreaming(const std::wstring& file, const std::wstring& args,
                              OutputLineFn onLine) {
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

    if (!CreateProcessW(file.c_str(), cmdBuf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                        nullptr, nullptr, &si, &pi)) {
        if (logf::Active()) {
            logf::Error("CreateProcess failed (" + util::ToUtf8(file) +
                        "): Win32=" + std::to_string(GetLastError()));
        }
        CloseHandle(outWr);
        CloseHandle(errWr);
        CloseHandle(outRd);
        CloseHandle(errRd);
        return r;
    }
    CloseHandle(outWr);
    CloseHandle(errWr);

    std::string pendingOut;
  {
        char buf[4096];
        for (;;) {
            DWORD wait = WaitForSingleObject(pi.hProcess, 50);
            DWORD avail = 0;
            while (PeekNamedPipe(outRd, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                DWORD read = 0;
                if (!ReadFile(outRd, buf, sizeof(buf) - 1, &read, nullptr) || read == 0) break;
                EmitOutputLines(pendingOut, buf, read, onLine, r.output);
            }
            DWORD errAvail = 0;
            while (PeekNamedPipe(errRd, nullptr, 0, nullptr, &errAvail, nullptr) && errAvail > 0) {
                DWORD read = 0;
                if (!ReadFile(errRd, buf, sizeof(buf) - 1, &read, nullptr) || read == 0) break;
                buf[read] = 0;
                r.error += buf;
            }
            if (wait == WAIT_OBJECT_0) break;
        }
        for (;;) {
            DWORD read = 0;
            if (!ReadFile(outRd, buf, sizeof(buf) - 1, &read, nullptr) || read == 0) break;
            EmitOutputLines(pendingOut, buf, read, onLine, r.output);
        }
        if (!pendingOut.empty() && onLine) onLine(pendingOut);
    }

    DWORD code{};
    GetExitCodeProcess(pi.hProcess, &code);
    r.exitCode = static_cast<int>(code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(outRd);
    CloseHandle(errRd);
    LogProcess("run", file, args, r);
    return r;
}

RunResult RunElevated(const std::wstring& file, const std::wstring& args) {
    RunResult r;
    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    sei.lpVerb = L"runas";
    sei.lpFile = file.c_str();
    sei.lpParameters = args.empty() ? nullptr : args.c_str();
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) return r;
    if (!sei.hProcess) return r;

    WaitForSingleObject(sei.hProcess, INFINITE);
    DWORD code{};
    GetExitCodeProcess(sei.hProcess, &code);
    r.exitCode = static_cast<int>(code);
    CloseHandle(sei.hProcess);
    LogProcess("elevated", file, args, r);
    return r;
}

RunResult RunHidden(const std::string& commandLine) {
    return Run(L"cmd.exe", L"/C " + maku::util::ToWide(commandLine));
}

RunResult RunHiddenElevated(const std::string& commandLine) {
    return RunElevated(L"cmd.exe", L"/C " + maku::util::ToWide(commandLine));
}

static std::wstring EncodePowerShellUtf16Le(const std::wstring& script) {
    if (script.empty()) return {};
    const DWORD byteLen = static_cast<DWORD>(script.size() * sizeof(wchar_t));
    DWORD b64Len = 0;
    if (!CryptBinaryToStringW(reinterpret_cast<const BYTE*>(script.data()), byteLen,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64Len))
        return {};
    std::wstring b64(b64Len, L'\0');
    if (!CryptBinaryToStringW(reinterpret_cast<const BYTE*>(script.data()), byteLen,
                              CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64.data(), &b64Len))
        return {};
    while (!b64.empty() && b64.back() == L'\0') b64.pop_back();
    return b64;
}

static RunResult RunPowerShellScript(const std::wstring& script, OutputLineFn onLine) {
    if (logf::Active())
        logf::Info("PowerShell script (" + std::to_string(script.size()) + " wchar_t)");

    const std::wstring psExe = PowerShellExePath();
    const std::wstring ps1 = WriteTempScript(script);
    if (ps1.empty()) {
        logf::Error("RunPowerShell: failed to write temp script");
        return {};
    }
    const std::wstring args =
        L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + ps1 + L"\"";
    RunResult r = onLine ? RunStreaming(psExe, args, onLine) : Run(psExe, args);
    DeleteFileW(ps1.c_str());
    if (r.exitCode == -1) {
        logf::Warn("PowerShell -File failed, trying -EncodedCommand fallback");
        const std::wstring enc = EncodePowerShellUtf16Le(script);
        if (enc.empty()) {
            logf::Error("RunPowerShell: failed to encode script");
            return {};
        }
        r = Run(psExe, L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + enc);
    }
    return r;
}

RunResult RunPowerShell(const std::wstring& script) { return RunPowerShellScript(script, nullptr); }

RunResult RunPowerShellElevated(const std::wstring& script) {
    const std::wstring psExe = PowerShellExePath();
    const std::wstring ps1 = WriteTempScript(script);
    if (!ps1.empty()) {
        const std::wstring args =
            L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + ps1 + L"\"";
        RunResult r = RunElevated(psExe, args);
        DeleteFileW(ps1.c_str());
        if (r.exitCode != -1) return r;
    }
    const std::wstring enc = EncodePowerShellUtf16Le(script);
    if (enc.empty()) return {};
    return RunElevated(psExe,
                       L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " + enc);
}

RunResult RunPowerShellAdmin(const std::wstring& script) {
    if (os::IsElevated()) return RunPowerShell(script);
    if (os::HasAdminRights()) return RunPowerShellElevated(script);
    return {};
}

RunResult RunPowerShellAdminStreaming(const std::wstring& script, OutputLineFn onLine) {
    if (os::IsElevated()) return RunPowerShellScript(script, onLine);
    if (os::HasAdminRights()) return RunPowerShellElevated(script);
    return {};
}

RunResult RunAdmin(const std::wstring& file, const std::wstring& args) {
    if (os::IsElevated()) return Run(file, args);
    if (os::HasAdminRights()) return RunElevated(file, args);
    return {};
}

RunResult RunHiddenAdmin(const std::string& commandLine) {
    if (!os::IsElevated()) return {};
    return RunHidden(commandLine);
}

void OpenUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static std::wstring WindowsDirFile(const wchar_t* name) {
    wchar_t dir[MAX_PATH]{};
    if (GetWindowsDirectoryW(dir, MAX_PATH) == 0) return name;
    return std::wstring(dir) + L"\\" + name;
}

static bool StartGuiProcess(const std::wstring& path, const wchar_t* params = nullptr) {
    const HINSTANCE r =
        ShellExecuteW(nullptr, L"open", path.c_str(), params, nullptr, SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(r) > 32) return true;
    if (logf::Active()) {
        logf::Error("ShellExecute failed (" + util::ToUtf8(path) + ") code=" +
                    std::to_string(static_cast<int>(reinterpret_cast<INT_PTR>(r))));
    }
    return false;
}

void KillExplorer() {
    const std::wstring taskkill = WindowsDirFile(L"System32\\taskkill.exe");
    Run(taskkill, L"/F /IM explorer.exe");
    logf::Info("Kill explorer (taskkill)");
}

void StartExplorer() {
    const std::wstring explorer = WindowsDirFile(L"explorer.exe");
    if (StartGuiProcess(explorer)) logf::Info("Start explorer");
}

void RestartExplorer() {
    KillExplorer();
    Sleep(1500);
    StartExplorer();
}

} // namespace maku::proc
