#include "core/Rollback.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <windows.h>

namespace maku::rollback {
namespace {

const wchar_t* kKeys[] = {
    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
    L"HKLM\\SOFTWARE\\Policies\\Microsoft\\FVE",
    L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
    L"HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ReserveManager",
    L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
    L"HKLM\\SYSTEM\\CurrentControlSet\\Services\\WSearch",
    L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
    L"HKLM\\SYSTEM\\CurrentControlSet\\Control\\Session Manager",
    L"HKCU\\Software\\Policies\\Microsoft\\Windows\\Explorer",
    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
    L"HKCU\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
    L"HKCU\\Control Panel\\Accessibility\\StickyKeys",
};

proc::RunResult RunRegCmd(const std::wstring& args) {
    if (os::IsElevated()) return proc::Run(L"reg.exe", args);
    return proc::RunElevated(L"reg.exe", args);
}

proc::RunResult RunPsElevated(const std::wstring& script) {
    if (os::IsElevated()) return proc::RunPowerShell(script);
    return proc::RunPowerShellElevated(script);
}

} // namespace

std::wstring BackupDir() { return util::GetAppDataPath() + L"\\rollback"; }

bool CreateSnapshot(const std::wstring& label) { return CreateSnapshot(label, std::wstring{}); }

bool CreateSnapshot(const std::wstring& label, const std::wstring& note) {
    namespace fs = std::filesystem;
    const std::wstring dir = BackupDir() + L"\\" + label;
    std::error_code ec;
    fs::create_directories(dir, ec);
    for (size_t i = 0; i < std::size(kKeys); ++i) {
        std::wstring file = dir + L"\\key" + std::to_wstring(i) + L".reg";
        wchar_t cmd[512];
        swprintf_s(cmd, L"export \"%s\" \"%s\" /y", kKeys[i], file.c_str());
        RunRegCmd(cmd);
    }
    std::wofstream marker(dir + L"\\snapshot.txt");
    if (!note.empty()) marker << note << L"\n";
    marker << L"ok\n";
    return true;
}

void SnapshotBeforeElevation() {
    static bool done = false;
    if (done) return;
    done = true;
    CreateSnapshot(L"pre_uac",
                   L"Auto-backup before administrator elevation — restore after reboot if needed.");
}

void SnapshotBeforeReboot() {
    static bool done = false;
    if (done) return;
    done = true;
    CreateSnapshot(L"pre_reboot",
                   L"Auto-backup before reboot — pending system tweaks can be reverted here.");
}

std::wstring LatestSnapshotPath() {
    namespace fs = std::filesystem;
    std::wstring latest;
    fs::file_time_type best{};
    const std::wstring root = BackupDir();
    std::error_code ec;
    if (!fs::exists(root, ec)) return {};
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory()) continue;
        if (fs::exists(e.path() / L"snapshot.txt", ec)) {
            const auto t = fs::last_write_time(e.path(), ec);
            if (latest.empty() || t > best) {
                best = t;
                latest = e.path().wstring();
            }
        }
    }
    return latest;
}

bool HasSnapshot() { return !LatestSnapshotPath().empty(); }

bool CreateSystemRestorePoint() {
    return RunPsElevated(
               L"Checkpoint-Computer -Description 'MakuTweaker++' -RestorePointType MODIFY_SETTINGS")
               .exitCode == 0;
}

bool RestoreLatest() {
    const std::wstring dir = LatestSnapshotPath();
    if (dir.empty()) return false;
    namespace fs = std::filesystem;
    for (const auto& e : fs::directory_iterator(dir)) {
        if (e.path().extension() == L".reg") {
            wchar_t cmd[512];
            swprintf_s(cmd, L"import \"%s\"", e.path().wstring().c_str());
            RunRegCmd(cmd);
        }
    }
    return true;
}

} // namespace maku::rollback
