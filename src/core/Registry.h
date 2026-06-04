#pragma once
#include <windows.h>
#include <optional>
#include <string>
#include <vector>

namespace maku::reg {

enum class Hive { LocalMachine, CurrentUser };

inline HKEY ToHkey(Hive h) {
    return h == Hive::LocalMachine ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

bool KeyExists(HKEY root, const std::wstring& subKey, REGSAM sam = KEY_READ);
bool DeleteKeyTree(HKEY root, const std::wstring& subKey);
bool CreateKey(HKEY root, const std::wstring& subKey);

bool GetDword(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD& out,
              REGSAM sam = KEY_READ);
bool SetDword(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD value);

bool GetString(HKEY root, const std::wstring& subKey, const std::wstring& name, std::wstring& out,
               REGSAM sam = KEY_READ);
bool SetString(HKEY root, const std::wstring& subKey, const std::wstring& name,
               const std::wstring& value);

std::optional<DWORD> TryGetDword(HKEY root, const std::wstring& subKey, const std::wstring& name);

} // namespace maku::reg
