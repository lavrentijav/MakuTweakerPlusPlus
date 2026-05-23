#include "core/ShellMenu.h"
#include "core/Registry.h"
#include "core/StringUtil.h"

namespace maku::shellmenu {
namespace {

const wchar_t* kBase = L"Software\\Classes";
std::wstring ExeQuoted() { return L"\"" + util::GetExePath() + L"\""; }

bool SetCmd(const std::wstring& sub, const std::wstring& args) {
    const std::wstring key = std::wstring(kBase) + L"\\" + sub;
    if (!reg::CreateKey(HKEY_CURRENT_USER, key)) return false;
    return reg::SetString(HKEY_CURRENT_USER, key, L"", ExeQuoted() + L" " + args);
}

} // namespace

bool Install() {
    const std::wstring kill = L"*\\shell\\MakuKill\\command";
    const std::wstring killL = L"*\\shell\\MakuKill";
    reg::SetString(HKEY_CURRENT_USER, killL, L"", L"End process (Maku)");
    SetCmd(kill, L"--kill-from-path \"%1\"");

    const std::wstring clean = L"Directory\\shell\\MakuCleanDir\\command";
    const std::wstring cleanL = L"Directory\\shell\\MakuCleanDir";
    reg::SetString(HKEY_CURRENT_USER, cleanL, L"", L"Clean folder before delete (Maku)");
    SetCmd(clean, L"--clean-folder \"%1\"");

    return true;
}

bool Uninstall() {
    reg::DeleteKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\MakuKill");
    reg::DeleteKeyTree(HKEY_CURRENT_USER, L"Software\\Classes\\Directory\\shell\\MakuCleanDir");
    return true;
}

bool IsInstalled() {
    return reg::KeyExists(HKEY_CURRENT_USER, L"Software\\Classes\\*\\shell\\MakuKill\\command");
}

} // namespace maku::shellmenu
