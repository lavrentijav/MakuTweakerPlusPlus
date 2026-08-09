#include "core/Registry.h"

namespace maku::reg {

static bool Open(HKEY root, const std::wstring& subKey, REGSAM sam, HKEY& key) {
    return RegOpenKeyExW(root, subKey.c_str(), 0, sam, &key) == ERROR_SUCCESS;
}

bool KeyExists(HKEY root, const std::wstring& subKey, REGSAM sam) {
    HKEY key{};
    if (!Open(root, subKey, sam, key)) return false;
    RegCloseKey(key);
    return true;
}

bool DeleteKeyTree(HKEY root, const std::wstring& subKey) {
    return RegDeleteTreeW(root, subKey.c_str()) == ERROR_SUCCESS;
}

bool CreateKey(HKEY root, const std::wstring& subKey) {
    HKEY key{};
    DWORD disp{};
    LONG r = RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, &disp);
    if (r == ERROR_SUCCESS) RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool GetDword(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD& out, REGSAM sam) {
    HKEY key{};
    if (!Open(root, subKey, sam, key)) return false;
    DWORD type{}, size = sizeof(DWORD);
    LONG r = RegQueryValueExW(key, name.c_str(), nullptr, &type, reinterpret_cast<LPBYTE>(&out), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD;
}

bool SetDword(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD value) {
    if (!CreateKey(root, subKey)) {
        HKEY key{};
        if (!Open(root, subKey, KEY_SET_VALUE, key)) return false;
        LONG r = RegSetValueExW(key, name.c_str(), 0, REG_DWORD,
                               reinterpret_cast<const BYTE*>(&value), sizeof(value));
        RegCloseKey(key);
        return r == ERROR_SUCCESS;
    }
    HKEY key{};
    if (!Open(root, subKey, KEY_SET_VALUE, key)) return false;
    LONG r = RegSetValueExW(key, name.c_str(), 0, REG_DWORD,
                           reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool GetString(HKEY root, const std::wstring& subKey, const std::wstring& name, std::wstring& out,
               REGSAM sam) {
    HKEY key{};
    if (!Open(root, subKey, sam, key)) return false;
    DWORD type{}, size = 0;
    if (RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ)) {
        RegCloseKey(key);
        return false;
    }
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(key, name.c_str(), nullptr, &type,
                         reinterpret_cast<LPBYTE>(buf.data()), &size) != ERROR_SUCCESS) {
        RegCloseKey(key);
        return false;
    }
    RegCloseKey(key);
    out = buf.data();
    return true;
}

bool SetString(HKEY root, const std::wstring& subKey, const std::wstring& name,
               const std::wstring& value) {
    CreateKey(root, subKey);
    HKEY key{};
    if (!Open(root, subKey, KEY_SET_VALUE, key)) return false;
    const BYTE* data = reinterpret_cast<const BYTE*>(value.c_str());
    DWORD bytes = static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t));
    LONG r = RegSetValueExW(key, name.c_str(), 0, REG_SZ, data, bytes);
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool GetMultiString(HKEY root, const std::wstring& subKey, const std::wstring& name,
                    std::vector<std::wstring>& out, REGSAM sam) {
    HKEY key{};
    if (!Open(root, subKey, sam, key)) return false;
    DWORD type{}, size = 0;
    if (RegQueryValueExW(key, name.c_str(), nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        type != REG_MULTI_SZ) {
        RegCloseKey(key);
        return false;
    }
    std::vector<wchar_t> buf(size / sizeof(wchar_t) + 2, L'\0');
    LONG r = RegQueryValueExW(key, name.c_str(), nullptr, &type,
                              reinterpret_cast<LPBYTE>(buf.data()), &size);
    RegCloseKey(key);
    if (r != ERROR_SUCCESS) return false;

    out.clear();
    for (const wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) out.emplace_back(p);
    return true;
}

bool SetMultiString(HKEY root, const std::wstring& subKey, const std::wstring& name,
                    const std::vector<std::wstring>& values) {
    CreateKey(root, subKey);
    HKEY key{};
    if (!Open(root, subKey, KEY_SET_VALUE, key)) return false;

    // REG_MULTI_SZ is a run of NUL-terminated strings closed by one extra NUL.
    std::vector<wchar_t> blob;
    for (const auto& v : values) {
        blob.insert(blob.end(), v.begin(), v.end());
        blob.push_back(L'\0');
    }
    blob.push_back(L'\0');

    LONG r = RegSetValueExW(key, name.c_str(), 0, REG_MULTI_SZ,
                            reinterpret_cast<const BYTE*>(blob.data()),
                            static_cast<DWORD>(blob.size() * sizeof(wchar_t)));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool DeleteValue(HKEY root, const std::wstring& subKey, const std::wstring& name) {
    HKEY key{};
    if (!Open(root, subKey, KEY_SET_VALUE, key)) return false;
    LONG r = RegDeleteValueW(key, name.c_str());
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

std::optional<DWORD> TryGetDword(HKEY root, const std::wstring& subKey, const std::wstring& name) {
    DWORD v{};
    if (GetDword(root, subKey, name, v)) return v;
    return std::nullopt;
}

std::optional<std::wstring> TryGetString(HKEY root, const std::wstring& subKey,
                                         const std::wstring& name) {
    std::wstring v;
    if (GetString(root, subKey, name, v)) return v;
    return std::nullopt;
}

bool DwordEquals(HKEY root, const std::wstring& subKey, const std::wstring& name, DWORD expected) {
    DWORD v{};
    return GetDword(root, subKey, name, v) && v == expected;
}

bool StringEquals(HKEY root, const std::wstring& subKey, const std::wstring& name,
                  const std::wstring& expected) {
    std::wstring v;
    return GetString(root, subKey, name, v) && v == expected;
}

} // namespace maku::reg
