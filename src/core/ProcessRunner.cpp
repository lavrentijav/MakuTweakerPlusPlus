#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <windows.h>
#include <shellapi.h>
#include <sstream>

namespace maku::proc {
namespace {

std::string NormalizePipeOutput(std::string raw) {
    if (raw.empty()) return raw;

    if (raw.size() >= 2 && static_cast<unsigned char>(raw[0]) == 0xFF &&
        static_cast<unsigned char>(raw[1]) == 0xFE) {
        const wchar_t* w = reinterpret_cast<const wchar_t*>(raw.data() + 2);
        const int wlen = static_cast<int>((raw.size() - 2) / sizeof(wchar_t));
        if (wlen <= 0) return {};
        const int need = WideCharToMultiByte(CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
        if (need <= 0) return {};
        std::string out(static_cast<size_t>(need), '\0');
        WideCharToMultiByte(CP_UTF8, 0, w, wlen, out.data(), need, nullptr, nullptr);
        return out;
    }

    size_t nullPairs = 0;
    for (size_t i = 1; i + 1 < raw.size(); i += 2) {
        if (raw[i] == '\0' && raw[i - 1] != '\0') ++nullPairs;
    }
    if (nullPairs > raw.size() / 6 && (raw.size() % 2) == 0) {
        const wchar_t* w = reinterpret_cast<const wchar_t*>(raw.data());
        const int wlen = static_cast<int>(raw.size() / sizeof(wchar_t));
        const int need = WideCharToMultiByte(CP_UTF8, 0, w, wlen, nullptr, 0, nullptr, nullptr);
        if (need > 0) {
            std::string out(static_cast<size_t>(need), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w, wlen, out.data(), need, nullptr, nullptr);
            return out;
        }
    }
    return raw;
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
    r.output = NormalizePipeOutput(std::move(r.output));
    r.error = NormalizePipeOutput(std::move(r.error));
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
    return r;
}

RunResult RunHidden(const std::string& commandLine) {
    return Run(L"cmd.exe", L"/C " + maku::util::ToWide(commandLine));
}

RunResult RunPowerShell(const std::wstring& script) {
    const std::wstring prefix =
        L"[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$OutputEncoding="
        L"[Console]::OutputEncoding;";
    return Run(L"powershell.exe",
               L"-NoProfile -NonInteractive -ExecutionPolicy Bypass -Command " + prefix + script);
}

void OpenUrl(const std::wstring& url) {
    ShellExecuteW(nullptr, L"open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

static std::wstring SystemExePath(const wchar_t* name) {
    wchar_t sysDir[MAX_PATH]{};
    if (!GetSystemDirectoryW(sysDir, MAX_PATH)) return name;
    return std::wstring(sysDir) + L"\\" + name;
}

bool LaunchDetached(const std::wstring& file, const std::wstring& args) {
    std::wstring path = file;
    if (path.find(L':') == std::wstring::npos && path.find(L'\\') == std::wstring::npos)
        path = SystemExePath(file.c_str());

    std::wstring cmdLine = L"\"" + path + L"\"";
    if (!args.empty()) cmdLine += L" " + args;

    STARTUPINFOW si{sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNORMAL;
    PROCESS_INFORMATION pi{};

    std::vector<wchar_t> cmdBuf(cmdLine.begin(), cmdLine.end());
    cmdBuf.push_back(0);

    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                        &pi))
        return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

void KillExplorer() {
    Run(SystemExePath(L"taskkill.exe"), L"/F /IM explorer.exe");
}

void RestartExplorer() {
    KillExplorer();
    Sleep(1500);
    if (!LaunchDetached(SystemExePath(L"explorer.exe"), L"")) {
        ShellExecuteW(nullptr, nullptr, SystemExePath(L"explorer.exe").c_str(), nullptr, nullptr,
                      SW_SHOWNORMAL);
    }
}

} // namespace maku::proc
