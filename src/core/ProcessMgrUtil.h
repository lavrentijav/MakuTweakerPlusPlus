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

void RefreshProcesses(std::vector<ProcRow>& out);
void RefreshServices(std::vector<SvcRow>& out);

bool MatchesSearch(const std::wstring& haystack, const std::wstring& query);
bool IsExcluded(const std::wstring& exeName, const std::string& exclusionsCsv);

std::wstring ServiceStateText(DWORD state);
std::wstring ServiceStartTypeText(DWORD startType);

} // namespace maku::pmgr
