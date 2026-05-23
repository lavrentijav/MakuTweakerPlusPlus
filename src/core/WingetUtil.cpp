#include "core/WingetUtil.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"

namespace maku::winget {

std::vector<AppEntry> Catalog() {
    return {
        {"VLC.vlc", "VLC"},
        {"OBSProject.OBSStudio", "OBS Studio"},
        {"Valve.Steam", "Steam"},
        {"Discord.Discord", "Discord"},
        {"Google.Chrome", "Google Chrome"},
        {"7zip.7zip", "7-Zip"},
        {"Notepad++.Notepad++", "Notepad++"},
        {"Git.Git", "Git"},
        {"Microsoft.PowerToys", "PowerToys"},
    };
}

bool IsAvailable() {
    return proc::Run(L"where", L"winget").exitCode == 0;
}

bool Install(const std::string& id, std::string& logOut) {
    const std::wstring args = L"install -e --id " + util::ToWide(id) +
                              L" --accept-package-agreements --accept-source-agreements";
    const auto r = proc::Run(L"winget", args);
    logOut = r.output + r.error;
    return r.exitCode == 0;
}

} // namespace maku::winget
