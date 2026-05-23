#pragma once
#include <string>
#include <vector>

namespace maku::wmi {

struct Row {
    std::wstring name;
    std::wstring value;
};

bool Query(const std::wstring& wql, std::vector<std::vector<Row>>& rows,
           const wchar_t* wmiNamespace = L"ROOT\\CIMV2");
std::wstring QueryScalar(const std::wstring& wql, const std::wstring& property);
std::vector<std::wstring> QueryList(const std::wstring& wql, const std::wstring& property);

} // namespace maku::wmi
