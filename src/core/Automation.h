#pragma once

namespace maku::auto_task {

bool InstallBootTtl();
bool RemoveBootTtl();
bool InstallWeeklyCleanup();
bool RemoveWeeklyCleanup();
bool IsBootTtlInstalled();
bool IsWeeklyCleanupInstalled();

void RunTempCleanup();

} // namespace maku::auto_task
