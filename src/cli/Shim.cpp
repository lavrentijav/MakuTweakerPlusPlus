// Console launcher for the CLI.
//
// MakuTweaker++.exe is linked /SUBSYSTEM:WINDOWS so the GUI never flashes a
// console window. The cost is that shells do not wait for it: `MakuTweaker++
// tweak list` returns to the prompt immediately, output arrives after the
// prompt is redrawn, and %ERRORLEVEL% is meaningless.
//
// This shim is the usual answer (npm, VS Code and friends ship the same trick):
// a tiny /SUBSYSTEM:CONSOLE binary named MakuTweaker++.com. Windows prefers
// .com over .exe when a bare name is typed, so `MakuTweaker++ <command>` runs
// this, which starts the real executable with the same arguments and inherited
// handles, waits for it, and returns its exit code.

#include <windows.h>

#include <string>

namespace {

/// Full path to the sibling .exe. Derived from our own path so a renamed or
/// relocated install still works.
std::wstring RealExecutablePath() {
    wchar_t self[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, self, MAX_PATH) == 0) return {};

    std::wstring path = self;
    const size_t dot = path.find_last_of(L'.');
    const size_t slash = path.find_last_of(L"\\/");
    if (dot != std::wstring::npos && (slash == std::wstring::npos || dot > slash))
        path.resize(dot);
    path += L".exe";
    return path;
}

/// Everything after the program name in our own command line, verbatim.
/// Re-quoting parsed argv would corrupt arguments containing spaces or quotes.
const wchar_t* ArgumentTail() {
    const wchar_t* p = GetCommandLineW();
    if (!p) return L"";

    if (*p == L'"') {
        ++p;
        while (*p && *p != L'"') ++p;
        if (*p == L'"') ++p;
    } else {
        while (*p && *p != L' ' && *p != L'\t') ++p;
    }
    while (*p == L' ' || *p == L'\t') ++p;
    return p;
}

} // namespace

int wmain() {
    const std::wstring exe = RealExecutablePath();
    if (exe.empty()) {
        fwprintf(stderr, L"MakuTweaker++: cannot locate the main executable\n");
        return 1;
    }

    std::wstring commandLine = L"\"" + exe + L"\"";
    const wchar_t* tail = ArgumentTail();
    if (*tail) {
        commandLine += L" ";
        commandLine += tail;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    // Inherit handles and stay in this console so redirection and piping work.
    if (!CreateProcessW(exe.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr,
                        nullptr, &si, &pi)) {
        fwprintf(stderr, L"MakuTweaker++: cannot start %ls (error %lu)\n", exe.c_str(),
                 GetLastError());
        return 1;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exitCode);
}
