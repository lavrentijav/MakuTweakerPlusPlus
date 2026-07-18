#pragma once
#include <cstdio>
#include <string>

namespace maku::sysfmt {

inline std::string FormatCimDate(const std::string& raw) {
    if (raw.size() < 8) return raw;
    auto digit = [&](size_t i) -> int {
        if (i >= raw.size() || raw[i] < '0' || raw[i] > '9') return -1;
        return raw[i] - '0';
    };
    const int y1 = digit(0), y2 = digit(1), y3 = digit(2), y4 = digit(3);
    const int m1 = digit(4), m2 = digit(5);
    const int d1 = digit(6), d2 = digit(7);
    if (y1 < 0 || m1 < 0 || d1 < 0) return raw;
    char buf[32]{};
    snprintf(buf, sizeof(buf), "%02d.%02d.%04d", d1 * 10 + d2, m1 * 10 + m2,
             y1 * 1000 + y2 * 100 + y3 * 10 + y4);
    return buf;
}

inline bool ParseUint64Wide(const std::wstring& s, unsigned long long& out) {
    if (s.empty()) return false;
    try {
        out = std::stoull(s);
        return true;
    } catch (...) {
        return false;
    }
}

inline std::wstring FormatBytesHuman(unsigned long long bytes) {
    if (bytes == 0) return L"0 B";
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double v = static_cast<double>(bytes);
    int unit = 0;
    while (v >= 1024.0 && unit < 4) {
        v /= 1024.0;
        ++unit;
    }
    wchar_t buf[64]{};
    swprintf_s(buf, unit == 0 ? L"%.0f %s" : L"%.2f %s", v, units[unit]);
    return buf;
}

inline std::wstring FormatAdapterVram(const std::wstring& adapterRamRaw) {
    unsigned long long v = 0;
    if (!ParseUint64Wide(adapterRamRaw, v)) return {};
    if (v == 0 || v >= 0xFFFFFFFF00000000ULL) return {};
    return FormatBytesHuman(v);
}

inline std::wstring MemoryTypeName(int code) {
    switch (code) {
    case 20: return L"DDR";
    case 21: return L"DDR2";
    case 22: return L"DDR2 FB-DIMM";
    case 24: return L"DDR3";
    case 26: return L"DDR4";
    case 27: return L"LPDDR";
    case 28: return L"LPDDR2";
    case 29: return L"LPDDR3";
    case 30: return L"LPDDR4";
    case 32: return L"HBM";
    case 33: return L"HBM2";
    case 34: return L"DDR5";
    case 35: return L"LPDDR5";
    case 36: return L"HBM3";
    default: return {};
    }
}

inline std::wstring MemoryTypeFromWmi(const std::wstring& smbios, const std::wstring& legacy,
                                     int avgSpeedMhz) {
    int code = 0;
    unsigned long long tmp = 0;
    if (ParseUint64Wide(smbios, tmp)) code = static_cast<int>(tmp);
    if (code <= 2 && ParseUint64Wide(legacy, tmp)) code = static_cast<int>(tmp);
    if (auto name = MemoryTypeName(code); !name.empty()) return name;
    if (avgSpeedMhz >= 4800) return L"DDR5";
    if (avgSpeedMhz >= 2133) return L"DDR4";
    if (avgSpeedMhz >= 800) return L"DDR3";
    if (avgSpeedMhz >= 400) return L"DDR2";
    if (avgSpeedMhz > 0) return L"DDR";
    return L"N/A";
}

} // namespace maku::sysfmt
