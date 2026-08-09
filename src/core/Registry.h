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

bool GetMultiString(HKEY root, const std::wstring& subKey, const std::wstring& name,
                    std::vector<std::wstring>& out, REGSAM sam = KEY_READ);
bool SetMultiString(HKEY root, const std::wstring& subKey, const std::wstring& name,
                    const std::vector<std::wstring>& values);

bool DeleteValue(HKEY root, const std::wstring& subKey, const std::wstring& name);

std::optional<DWORD> TryGetDword(HKEY root, const std::wstring& subKey, const std::wstring& name);
std::optional<std::wstring> TryGetString(HKEY root, const std::wstring& subKey,
                                         const std::wstring& name);

/// True when the DWORD exists and equals `expected`.
bool DwordEquals(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD expected);
/// True when the string value exists and equals `expected` (case-sensitive).
bool StringEquals(HKEY root, const std::wstring& subKey, const std::wstring& name,
                  const std::wstring& expected);

} // namespace maku::reg
