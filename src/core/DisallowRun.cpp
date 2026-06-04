#include "core/DisallowRun.h"
#include "core/Registry.h"
#include "core/StringUtil.h"
#include <algorithm>
#include <sstream>

namespace maku::disallow {
namespace {

bool IsForbidden(const std::string& token) {
    static const char* forbidden[] = {
        "makutweaker", "explorer", "taskmgr", "dwm", "csrss", "winlogon", "lsass", "svchost",
        "regedit", "cmd", "powershell", "pwsh", "system", "idle"};
    for (auto f : forbidden)
        if (token == f) return true;
    return false;
}

std::vector<std::string> ParseList(const std::string& input) {
    std::vector<std::string> out;
    std::stringstream ss(input);
    std::string part;
    while (std::getline(ss, part, ',')) {
        for (auto& c : part) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        while (!part.empty() && part.front() == ' ') part.erase(part.begin());
        while (!part.empty() && part.back() == ' ') part.pop_back();
        if (!part.empty() && std::find(out.begin(), out.end(), part) == out.end()) out.push_back(part);
    }
    return out;
}

} // namespace

bool ContainsForbiddenProcess(const std::string& normalizedList) {
    for (const auto& p : ParseList(normalizedList)) {
        if (IsForbidden(p)) return true;
    }
    return false;
}

bool ApplyProcessBlockList(const std::string& commaSeparated, std::wstring* errorOut) {
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    const auto processes = ParseList(commaSeparated);
    for (const auto& p : processes) {
        if (IsForbidden(p)) {
            if (errorOut) *errorOut = L"Forbidden process in list";
            return false;
        }
    }
    if (!reg::SetDword(HKEY_CURRENT_USER, base, L"DisallowRun", 1)) {
        if (errorOut) *errorOut = L"Failed to set DisallowRun policy";
        return false;
    }
    const std::wstring sub = base + L"\\DisallowRun";
    reg::DeleteKeyTree(HKEY_CURRENT_USER, sub);
    reg::CreateKey(HKEY_CURRENT_USER, sub);
    for (size_t i = 0; i < processes.size(); ++i) {
        const std::wstring name = std::to_wstring(i + 1);
        reg::SetString(HKEY_CURRENT_USER, sub, name, util::ToWide(processes[i]));
    }
    return true;
}

} // namespace maku::disallow
