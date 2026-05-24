#pragma once

#include "app/Branding.h"

#include <shlobj.h>
#include <string>
#include <vector>
#include <windows.h>

namespace maku::util {

inline std::wstring ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring out(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, out.data(), size);
    return out;
}

inline std::string ToUtf8(const std::wstring& wide) {
    if (wide.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, out.data(), size, nullptr, nullptr);
    return out;
}

inline std::wstring GetExePath() {
    wchar_t path[MAX_PATH]{};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return path;
}

inline std::wstring GetExeDirectory() {
    std::wstring s = GetExePath();
    auto pos = s.find_last_of(L"\\/");
    if (pos != std::wstring::npos) s.resize(pos);
    return s;
}

inline std::wstring GetAppDataPath() {
    wchar_t* path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
        return L"";
    std::wstring result = path;
    CoTaskMemFree(path);
    result += L"\\";
    result += brand::kAppDataFolder;
    CreateDirectoryW(result.c_str(), nullptr);
    return result;
}

/// Shared data directory (ProgramData) — same path for GUI and Windows service.
inline std::wstring GetSharedDataPath() {
    wchar_t* path = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData, 0, nullptr, &path)))
        return L"";
    std::wstring result = path;
    CoTaskMemFree(path);
    result += L"\\";
    result += brand::kAppDataFolder;
    CreateDirectoryW(result.c_str(), nullptr);
    return result;
}

} // namespace maku::util
