#pragma once
#include <string>
#include <vector>
#include <windows.h>

namespace maku::pmgr {

struct ProcRow {
    DWORD pid = 0;
    std::wstring name;
    std::wstring path;
    SIZE_T mem = 0;
    bool frozen = false;
    bool critical = false;
};

struct SvcRow {
    std::wstring name;
    std::wstring displayName;
    DWORD state = 0;
    DWORD startType = 0;
    bool critical = false;
};

/// @param detectFrozen if false, skips hung-window scan (much faster)
void RefreshProcesses(std::vector<ProcRow>& out, bool detectFrozen = false);
void RefreshServices(std::vector<SvcRow>& out);

bool TerminateProcessByPid(DWORD pid);
void TerminateProcessesByName(const std::wstring& exeName);
bool OpenProcessLocation(const std::wstring& path);

bool MatchesSearch(const std::wstring& haystack, const std::wstring& query);
bool IsExcluded(const std::wstring& exeName, const std::string& exclusionsCsv);

std::wstring ServiceStateText(DWORD state);
std::wstring ServiceStartTypeText(DWORD startType);

} // namespace maku::pmgr
