#pragma once
#include <string>

namespace maku::rollback {

std::wstring BackupDir();
bool CreateSnapshot(const std::wstring& label);
bool CreateSnapshot(const std::wstring& label, const std::wstring& note);
/// Registry export before UAC relaunch (once per process).
void SnapshotBeforeElevation();
/// Registry export before first reboot notification this session.
void SnapshotBeforeReboot();
bool RestoreLatest();
bool CreateSystemRestorePoint();
bool HasSnapshot();
std::wstring LatestSnapshotPath();

} // namespace maku::rollback
