#include "core/TaskMgrReplace.h"
#include "core/Registry.h"
#include "core/StringUtil.h"

namespace maku::taskmgr {
namespace {

const std::wstring kIfeo =
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Image File Execution Options\\taskmgr.exe";

} // namespace

bool IsReplaced() {
    std::wstring dbg;
    return reg::GetString(HKEY_LOCAL_MACHINE, kIfeo, L"Debugger", dbg) && !dbg.empty();
}

bool SetReplace(bool enable, std::wstring* errorOut) {
    if (enable) {
        const std::wstring exe = L"\"" + util::GetExePath() + L"\" /mgr";
        if (!reg::CreateKey(HKEY_LOCAL_MACHINE, kIfeo)) {
            if (errorOut) *errorOut = L"Administrator rights required to replace Task Manager";
            return false;
        }
        if (!reg::SetString(HKEY_LOCAL_MACHINE, kIfeo, L"Debugger", exe)) {
            if (errorOut) *errorOut = L"Failed to set IFEO Debugger (run as administrator)";
            return false;
        }
        return true;
    }
    reg::DeleteKeyTree(HKEY_LOCAL_MACHINE, kIfeo);
    return true;
}

} // namespace maku::taskmgr
