#include "core/OsUtil.h"
#include "app/Branding.h"
#include "core/LogFile.h"
#include "core/StringUtil.h"

#include <windows.h>
#include <shellapi.h>

namespace maku::os {
namespace {

HWND g_elevationOwner = nullptr;

std::wstring CommandLineParamsOnly() {
    wchar_t* cmd = GetCommandLineW();
    if (!cmd || !*cmd) return {};

    if (*cmd == L'"') {
        ++cmd;
        while (*cmd && *cmd != L'"') ++cmd;
        if (*cmd == L'"') ++cmd;
    } else {
        while (*cmd && !iswspace(*cmd)) ++cmd;
    }
    while (*cmd && iswspace(*cmd)) ++cmd;
    return cmd;
}

} // namespace

void SetElevationOwner(HWND hwnd) { g_elevationOwner = hwnd; }

bool IsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elev{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elev, sizeof(elev), &size) != 0 &&
                    elev.TokenIsElevated;
    CloseHandle(token);
    return ok != FALSE;
}

bool IsRunningAsAdmin() {
    BOOL admin = FALSE;
    PSID group = nullptr;
    SID_IDENTIFIER_AUTHORITY auth = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&auth, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,
                                 0, 0, 0, 0, 0, &group)) {
        CheckTokenMembership(nullptr, group, &admin);
        FreeSid(group);
    }
    return admin != FALSE;
}

bool HasAdminRights() { return IsElevated() || IsRunningAsAdmin(); }

bool RequestElevation(HWND owner) {
    if (IsElevated()) return true;

    if (!owner) owner = g_elevationOwner;

    const std::wstring msg =
        L"Для этой настройки нужны права администратора.\n\n"
        L"MakuTweaker++ будет полностью перезапущен с повышением прав (один запрос UAC). "
        L"После этого все системные твики применяются без повторных запросов.\n\n"
        L"Restart the entire app as administrator?";

    const int r =
        MessageBoxW(owner, msg.c_str(), brand::kDisplayName, MB_YESNO | MB_ICONWARNING | MB_TOPMOST);
    if (r != IDYES) return false;

    logf::Info("User requested elevation — relaunching with administrator rights");
    const std::wstring exe = util::GetExePath();
    const std::wstring params = CommandLineParamsOnly();

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = owner;
    sei.lpVerb = L"runas";
    sei.lpFile = exe.c_str();
    sei.lpParameters = params.empty() ? nullptr : params.c_str();
    sei.nShow = SW_SHOW;

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() == ERROR_CANCELLED) {
            MessageBoxW(owner, L"Повышение прав отменено.", L"MakuTweaker++",
                        MB_OK | MB_ICONINFORMATION);
        }
        return false;
    }

    if (sei.hProcess) CloseHandle(sei.hProcess);
    PostQuitMessage(0);
    return false;
}

bool EnsureAdmin(HWND owner) {
    if (IsElevated()) return true;
    return RequestElevation(owner);
}

} // namespace maku::os
