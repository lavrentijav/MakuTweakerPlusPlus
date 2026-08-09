#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <windows.h>
#include <shellapi.h>
#include <userenv.h>
#include <sstream>
#include <vector>

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

    // lpApplicationName must be NULL so the executable is resolved through
    // PATH. Passing a bare name like "powershell.exe" there makes CreateProcess
    // look only in the current directory, so every helper we shell out to
    // (powershell, dism, sfc, bcdedit, net, sc) failed with exit code -1 and
    // the callers silently treated that as "tweak is off".
    // cmdBuf already quotes the program, so a path with spaces is safe.
    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE, flags, nullptr, nullptr,
                        &si, &pi)) {
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

namespace {

/// A primary token copied from the running shell, or null.
///
/// MakuTweaker++ usually runs elevated, and Windows refuses to accept an
/// elevated process as the shell — starting explorer.exe with our own token
/// gives at best a stray file-browser window, which is why "restart Explorer"
/// looked like it only killed it. Grabbing the shell's own medium-integrity
/// token *before* it dies lets us start the replacement as the same user at the
/// same integrity level.
HANDLE CaptureShellToken() {
    const HWND shell = GetShellWindow();
    if (!shell) return nullptr;

    DWORD pid = 0;
    GetWindowThreadProcessId(shell, &pid);
    if (pid == 0) return nullptr;

    const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return nullptr;

    HANDLE processToken = nullptr;
    HANDLE primary = nullptr;
    if (OpenProcessToken(process, TOKEN_DUPLICATE, &processToken)) {
        if (!DuplicateTokenEx(processToken, TOKEN_ALL_ACCESS, nullptr, SecurityImpersonation,
                              TokenPrimary, &primary))
            primary = nullptr;
        CloseHandle(processToken);
    }
    CloseHandle(process);
    return primary;
}

bool StartExplorerWithToken(HANDLE token) {
    if (!token) return false;

    std::wstring path = SystemExePath(L"explorer.exe");
    std::vector<wchar_t> cmdBuf(path.begin(), path.end());
    cmdBuf.push_back(0);

    STARTUPINFOW si{sizeof(si)};
    si.lpDesktop = const_cast<wchar_t*>(L"winsta0\\default");
    PROCESS_INFORMATION pi{};

    // CreateProcessWithTokenW needs SeImpersonatePrivilege, which an elevated
    // admin process has; CreateProcessAsUserW covers the SYSTEM case.
    BOOL ok = CreateProcessWithTokenW(token, 0, path.c_str(), cmdBuf.data(), 0, nullptr, nullptr,
                                      &si, &pi);
    if (!ok)
        ok = CreateProcessAsUserW(token, path.c_str(), cmdBuf.data(), nullptr, nullptr, FALSE, 0,
                                  nullptr, nullptr, &si, &pi);
    if (!ok) return false;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

/// Asks the shell to exit the way the hidden "Exit Explorer" menu item does, so
/// it flushes its state instead of being torn down mid-write.
bool RequestShellExit() {
    const HWND tray = FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!tray) return false;
    constexpr UINT kExitExplorer = WM_USER + 436;
    return PostMessageW(tray, kExitExplorer, 0, 0) != FALSE;
}

bool ShellIsRunning() { return GetShellWindow() != nullptr; }

} // namespace

void RestartExplorer() {
    HANDLE shellToken = CaptureShellToken();

    if (!RequestShellExit()) {
        KillExplorer();
    } else {
        // Give the graceful path a moment, then force it.
        for (int i = 0; i < 30 && ShellIsRunning(); ++i) Sleep(100);
        if (ShellIsRunning()) KillExplorer();
    }

    // Winlogon's AutoRestartShell may bring the shell back on its own; wait
    // briefly and only start one ourselves if it does not.
    for (int i = 0; i < 20 && !ShellIsRunning(); ++i) Sleep(100);

    if (!ShellIsRunning()) {
        if (!StartExplorerWithToken(shellToken)) {
            // Not elevated (or no token): our own context is already correct.
            if (!LaunchDetached(SystemExePath(L"explorer.exe"), L""))
                ShellExecuteW(nullptr, nullptr, SystemExePath(L"explorer.exe").c_str(), nullptr,
                              nullptr, SW_SHOWNORMAL);
        }
    }

    if (shellToken) CloseHandle(shellToken);
}

} // namespace maku::proc
