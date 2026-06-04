#include "core/AppPaths.h"
#include "core/Registry.h"
#include "core/StringUtil.h"

namespace maku::apppaths {
namespace {

const wchar_t* kRoot = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths";
const wchar_t* kAliases[] = {L"makut.exe", L"maku.exe", L"mt.exe"};

} // namespace

bool WinRAliasesEnabled() {
    std::wstring val;
    return reg::GetString(HKEY_LOCAL_MACHINE, std::wstring(kRoot) + L"\\makut.exe", L"", val) &&
           !val.empty();
}

void SetWinRAliases(bool enable) {
    const std::wstring exe = util::GetExePath();
    for (auto alias : kAliases) {
        const std::wstring sub = std::wstring(kRoot) + L"\\" + alias;
        if (enable) {
            reg::CreateKey(HKEY_LOCAL_MACHINE, sub);
            reg::SetString(HKEY_LOCAL_MACHINE, sub, L"", exe);
        } else {
            reg::DeleteKeyTree(HKEY_LOCAL_MACHINE, sub);
        }
    }
}

} // namespace maku::apppaths
