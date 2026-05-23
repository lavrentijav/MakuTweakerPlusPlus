#include "core/SysInfo.h"
#include "core/Registry.h"
#include "core/Wmi.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#include <algorithm>
#pragma comment(lib, "iphlpapi.lib")
#include <cmath>
#include <cwctype>
#include <sstream>

namespace maku::pci {
namespace {

std::wstring RowVal(const std::vector<wmi::Row>& row, const wchar_t* name) {
    for (const auto& r : row)
        if (_wcsicmp(r.name.c_str(), name) == 0) return r.value;
    return L"";
}

std::wstring RegStr(HKEY root, const wchar_t* sub, const wchar_t* name) {
    std::wstring v;
    if (reg::GetString(root, sub, name, v)) return v;
    return L"";
}

std::optional<DWORD> RegDword(HKEY root, const wchar_t* sub, const wchar_t* name) {
    return reg::TryGetDword(root, sub, name);
}

std::wstring FormatWmiDate(const std::wstring& raw) {
    if (raw.size() < 8) return raw;
    return raw.substr(6, 2) + L"." + raw.substr(4, 2) + L"." + raw.substr(0, 4);
}


} // namespace

std::wstring FormatBytes(unsigned long long bytes) {
    if (bytes == 0) return L"0 B";
    const wchar_t* units[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double v = static_cast<double>(bytes);
    int u = 0;
    while (v >= 1024.0 && u < 4) {
        v /= 1024.0;
        ++u;
    }
    wchar_t buf[64];
    swprintf_s(buf, u >= 2 ? L"%.2f %s" : L"%.0f %s", v, units[u]);
    return buf;
}

namespace {

std::wstring DdrTypeName(int code) {
    switch (code) {
    case 20: return L"DDR";
    case 21: return L"DDR2";
    case 24: return L"DDR3";
    case 26: return L"DDR4";
    case 34: return L"DDR5";
    default: return code > 0 ? (L"Type " + std::to_wstring(code)) : L"—";
    }
}

std::wstring ActivationText() {
    std::vector<std::vector<wmi::Row>> rows;
    if (!wmi::Query(L"SELECT LicenseStatus FROM SoftwareLicensingProduct WHERE PartialProductKey IS NOT NULL",
                    rows)) {
        return L"—";
    }
    for (const auto& row : rows) {
        const std::wstring st = RowVal(row, L"LicenseStatus");
        if (st == L"1") return L"activated";
        if (st == L"0") return L"unlicensed";
    }
    return L"—";
}

std::wstring LocaleName(DWORD lcid) {
    wchar_t buf[128]{};
    // LOCALE_SENGLANGUAGE (0x1001) — stable across SDK targets
    if (GetLocaleInfoW(lcid, 0x00001001, buf, 128) > 0 && buf[0]) return buf;
    wchar_t lang[16]{};
    wchar_t region[16]{};
    GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, lang, 16);
    GetLocaleInfoW(lcid, LOCALE_SISO3166CTRYNAME, region, 16);
    return std::wstring(lang) + L" (" + region + L")";
}

void CollectWindows(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> rows;
    if (wmi::Query(L"SELECT Caption,Version,BuildNumber,OSArchitecture,InstallDate FROM Win32_OperatingSystem",
                   rows) &&
        !rows.empty()) {
        const auto& r = rows[0];
        s.osCaption = RowVal(r, L"Caption");
        s.displayVersion = RowVal(r, L"Version");
        s.build = RowVal(r, L"BuildNumber");
        s.arch = RowVal(r, L"OSArchitecture");
        s.installDate = FormatWmiDate(RowVal(r, L"InstallDate"));
    }

    const wchar_t* verKey = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    if (const std::wstring dv = RegStr(HKEY_LOCAL_MACHINE, verKey, L"DisplayVersion"); !dv.empty())
        s.displayVersion = dv;
    if (const std::wstring ed = RegStr(HKEY_LOCAL_MACHINE, verKey, L"EditionID"); !ed.empty())
        s.edition = ed;
    else if (const std::wstring pn = RegStr(HKEY_LOCAL_MACHINE, verKey, L"ProductName"); !pn.empty())
        s.edition = pn;

    if (const std::wstring ub = RegStr(HKEY_LOCAL_MACHINE, verKey, L"UBR"); !ub.empty())
        s.build += L"." + ub;

    const std::wstring act = ActivationText();
    if (act == L"activated") s.activation = L"__activated__";
    else if (act == L"unlicensed") s.activation = L"__unlicensed__";
    else s.activation = act;

    wchar_t comp[MAX_COMPUTERNAME_LENGTH + 1]{};
    DWORD compLen = MAX_COMPUTERNAME_LENGTH + 1;
    if (GetComputerNameW(comp, &compLen)) s.computerName = comp;

    wchar_t user[256]{};
    DWORD userLen = 256;
    if (GetUserNameW(user, &userLen)) s.username = user;
}

void CollectRegional(Snapshot& s) {
    s.uiLanguage = LocaleName(GetUserDefaultUILanguage());
    s.region = LocaleName(GetUserDefaultLCID());

    if (const std::wstring tz = wmi::QueryScalar(L"SELECT Caption FROM Win32_TimeZone", L"Caption");
        !tz.empty())
        s.timezone = tz;
}

void CollectSecurity(Snapshot& s) {
    const wchar_t* dg = L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard";
    const wchar_t* dgSc =
        L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity";

    if (const auto vbs = RegDword(HKEY_LOCAL_MACHINE, dg, L"EnableVirtualizationBasedSecurity"))
        s.vbs = *vbs ? L"__enabled__" : L"__disabled__";
    else
        s.vbs = L"__disabled__";

    if (const auto hvci = RegDword(HKEY_LOCAL_MACHINE, dgSc, L"Enabled"))
        s.wdac = *hvci ? L"__enabled_enforced__" : L"__disabled__";
    else
        s.wdac = L"__disabled__";

    std::vector<std::vector<wmi::Row>> feat;
    if (wmi::Query(
            L"SELECT InstallState FROM Win32_OptionalFeature WHERE Name='Microsoft-Hyper-V-All'", feat) &&
        !feat.empty()) {
        const std::wstring st = RowVal(feat[0], L"InstallState");
        s.hyperv = (st == L"1") ? L"__enabled__" : L"__disabled__";
    } else {
        s.hyperv = L"__not_found__";
    }

    if (const auto rt = RegDword(
            HKEY_LOCAL_MACHINE,
            L"SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection",
            L"DisableRealtimeMonitoring"))
        s.defender = *rt == 0 ? L"__enabled_rt__" : L"__disabled__";
    else
        s.defender = L"__enabled_rt__";

    if (const auto ci = RegDword(HKEY_LOCAL_MACHINE, dgSc, L"Enabled"))
        s.coreIsolation = *ci ? L"__enabled__" : L"__disabled__";
    else
        s.coreIsolation = L"__disabled__";

    if (const auto lua = RegDword(HKEY_LOCAL_MACHINE,
                                  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                                  L"EnableLUA"))
        s.uac = *lua ? L"__enabled__" : L"__disabled__";
    else
        s.uac = L"__enabled__";

    const std::wstring ss =
        RegStr(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer",
               L"SmartScreenEnabled");
    s.smartScreen =
        (ss.empty() || ss == L"Off") ? L"__disabled__" : L"__enabled__";

    std::vector<std::vector<wmi::Row>> tpmRows;
    if (wmi::Query(L"SELECT IsEnabled_InitialValue FROM Win32_Tpm", tpmRows,
                   L"ROOT\\CIMV2\\Security\\MicrosoftTpm") &&
        !tpmRows.empty()) {
        const std::wstring en = RowVal(tpmRows[0], L"IsEnabled_InitialValue");
        s.tpm = (en == L"1" || en == L"True") ? L"__present__" : L"__absent__";
    } else {
        s.tpm = L"__absent__";
    }

    if (const auto sb = RegDword(HKEY_LOCAL_MACHINE,
                                 L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State",
                                 L"UEFISecureBootEnabled"))
        s.secureBoot = *sb ? L"__enabled__" : L"__disabled__";
    else
        s.secureBoot = L"—";
}

// WMI Query overload - I need to add namespace support to Wmi.h or use separate connect in SysInfo

void CollectCpu(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> rows;
    if (!wmi::Query(L"SELECT Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed,L3CacheSize,"
                    L"AddressWidth FROM Win32_Processor",
                    rows)) {
        return;
    }
    for (const auto& row : rows) {
        if (s.cpuName.empty()) s.cpuName = RowVal(row, L"Name");
        s.cpuCores += _wtoi(RowVal(row, L"NumberOfCores").c_str());
        s.cpuThreads += _wtoi(RowVal(row, L"NumberOfLogicalProcessors").c_str());
        const int mhz = _wtoi(RowVal(row, L"MaxClockSpeed").c_str());
        if (mhz > static_cast<int>(s.cpuBaseGhz * 1000)) s.cpuBaseGhz = mhz / 1000.0;
        s.cpuL3Mb += _wtoi(RowVal(row, L"L3CacheSize").c_str()) / 1024;
        if (s.cpuArch.empty()) {
            const int bits = _wtoi(RowVal(row, L"AddressWidth").c_str());
            if (bits == 64) s.cpuArch = L"x64";
            else if (bits == 32) s.cpuArch = L"x86";
        }
    }
    if (s.cpuThreads == 0 || s.cpuThreads < s.cpuCores) {
        const std::wstring t = wmi::QueryScalar(
            L"SELECT NumberOfLogicalProcessors FROM Win32_ComputerSystem",
            L"NumberOfLogicalProcessors");
        if (!t.empty()) s.cpuThreads = _wtoi(t.c_str());
    }
}

void CollectRam(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> sticks;
    int typeCode = 0;
    int speedSum = 0;
    int speedCount = 0;
    if (wmi::Query(L"SELECT Capacity,MemoryType,SMBIOSMemoryType,Speed,Manufacturer,PartNumber "
                   L"FROM Win32_PhysicalMemory",
                   sticks)) {
        for (const auto& row : sticks) {
            RamStickInfo stick;
            stick.capacityBytes = _wcstoui64(RowVal(row, L"Capacity").c_str(), nullptr, 10);
            s.ramTotalBytes += stick.capacityBytes;
            const int smbios = _wtoi(RowVal(row, L"SMBIOSMemoryType").c_str());
            const int legacy = _wtoi(RowVal(row, L"MemoryType").c_str());
            const int detected = smbios > 2 ? smbios : (legacy > 2 ? legacy : 0);
            if (typeCode == 0 && detected != 0) typeCode = detected;
            stick.speedMhz = _wtoi(RowVal(row, L"Speed").c_str());
            if (stick.speedMhz > 0) {
                speedSum += stick.speedMhz;
                ++speedCount;
            }
            stick.manufacturer = RowVal(row, L"Manufacturer");
            stick.partNumber = RowVal(row, L"PartNumber");
            s.ramSticks.push_back(std::move(stick));
        }
    }
    if (s.ramTotalBytes == 0) {
        const std::wstring t = wmi::QueryScalar(
            L"SELECT TotalPhysicalMemory FROM Win32_ComputerSystem", L"TotalPhysicalMemory");
        s.ramTotalBytes = _wcstoui64(t.c_str(), nullptr, 10);
    }
    s.ramType = DdrTypeName(typeCode);
    if (speedCount > 0) s.ramSpeedMhz = speedSum / speedCount;
}

void CollectBoard(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> bb;
    if (wmi::Query(L"SELECT Manufacturer,Product FROM Win32_BaseBoard", bb) && !bb.empty()) {
        s.motherboard = RowVal(bb[0], L"Manufacturer") + L" " + RowVal(bb[0], L"Product");
        while (!s.motherboard.empty() && iswspace(s.motherboard.front()))
            s.motherboard.erase(s.motherboard.begin());
    }
    std::vector<std::vector<wmi::Row>> bios;
    if (wmi::Query(L"SELECT SMBIOSBIOSVersion,ReleaseDate FROM Win32_BIOS", bios) && !bios.empty()) {
        s.biosVersion = RowVal(bios[0], L"SMBIOSBIOSVersion");
        s.biosDate = FormatWmiDate(RowVal(bios[0], L"ReleaseDate"));
    }
    std::vector<std::vector<wmi::Row>> cs;
    if (wmi::Query(L"SELECT Manufacturer,Model FROM Win32_ComputerSystem", cs) && !cs.empty()) {
        s.pcManufacturer = RowVal(cs[0], L"Manufacturer");
        s.pcModel = RowVal(cs[0], L"Model");
        const wchar_t* invalid[] = {L"System Product Name", L"To Be Filled By O.E.M."};
        for (const wchar_t* inv : invalid) {
            if (_wcsicmp(s.pcModel.c_str(), inv) == 0) s.pcModel.clear();
        }
        if (_wcsicmp(s.pcManufacturer.c_str(), L"Unknown") == 0) s.pcManufacturer.clear();
    }
}

void CollectGpus(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> rows;
    if (!wmi::Query(L"SELECT Name,AdapterRAM,DriverVersion FROM Win32_VideoController", rows))
        return;
    for (const auto& row : rows) {
        GpuInfo g;
        g.name = RowVal(row, L"Name");
        if (g.name.empty()) continue;
        g.vramBytes = _wcstoui64(RowVal(row, L"AdapterRAM").c_str(), nullptr, 10);
        g.driverVersion = RowVal(row, L"DriverVersion");
        s.gpus.push_back(std::move(g));
    }
    std::sort(s.gpus.begin(), s.gpus.end(),
              [](const GpuInfo& a, const GpuInfo& b) { return a.vramBytes > b.vramBytes; });
}

void CollectStorage(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> disks;
    if (wmi::Query(L"SELECT Model,Size,MediaType,InterfaceType FROM Win32_DiskDrive", disks)) {
        for (const auto& row : disks) {
            DiskInfo d;
            d.model = RowVal(row, L"Model");
            d.sizeBytes = _wcstoui64(RowVal(row, L"Size").c_str(), nullptr, 10);
            d.mediaType = RowVal(row, L"MediaType");
            d.interfaceType = RowVal(row, L"InterfaceType");
            s.disks.push_back(std::move(d));
        }
    }
    std::sort(s.disks.begin(), s.disks.end(),
              [](const DiskInfo& a, const DiskInfo& b) { return a.sizeBytes > b.sizeBytes; });

    std::vector<std::vector<wmi::Row>> vols;
    if (wmi::Query(L"SELECT DeviceID,FileSystem,Size,FreeSpace FROM Win32_LogicalDisk WHERE DriveType=3",
                   vols)) {
        for (const auto& row : vols) {
            LogicalDriveInfo v;
            v.id = RowVal(row, L"DeviceID");
            v.fileSystem = RowVal(row, L"FileSystem");
            v.sizeBytes = _wcstoui64(RowVal(row, L"Size").c_str(), nullptr, 10);
            v.freeBytes = _wcstoui64(RowVal(row, L"FreeSpace").c_str(), nullptr, 10);
            s.volumes.push_back(std::move(v));
        }
    }
}

void CollectNetwork(Snapshot& s) {
    ULONG bufLen = 16 * 1024;
    std::vector<BYTE> buffer(bufLen);
    auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
    DWORD err = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &bufLen);
    if (err == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(bufLen);
        addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        err = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, addrs, &bufLen);
    }
    if (err != NO_ERROR) return;

    for (auto* a = addrs; a; a = a->Next) {
        if (a->OperStatus != IfOperStatusUp || a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        if (a->PhysicalAddressLength == 0) continue;

        NetAdapterInfo n;
        if (a->FriendlyName) n.description = a->FriendlyName;
        wchar_t mac[32]{};
        for (ULONG i = 0; i < a->PhysicalAddressLength && i < 8; ++i) {
            if (i) wcscat_s(mac, L":");
            wchar_t byte[4]{};
            swprintf_s(byte, L"%02X", a->PhysicalAddress[i]);
            wcscat_s(mac, byte);
        }
        n.mac = mac;

        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr) continue;
            wchar_t ip[64]{};
            DWORD ipLen = 64;
            if (u->Address.lpSockaddr->sa_family == AF_INET) {
                if (WSAAddressToStringW(u->Address.lpSockaddr, u->Address.iSockaddrLength, nullptr,
                                        ip, &ipLen) == 0 && n.ipv4.empty())
                    n.ipv4 = ip;
            } else if (u->Address.lpSockaddr->sa_family == AF_INET6) {
                if (WSAAddressToStringW(u->Address.lpSockaddr, u->Address.iSockaddrLength, nullptr,
                                        ip, &ipLen) == 0 &&
                    wcsstr(ip, L"fe80") != ip && n.ipv6.empty())
                    n.ipv6 = ip;
            }
        }
        if (!n.ipv4.empty() || !n.mac.empty()) s.netAdapters.push_back(std::move(n));
    }
}

void CollectBattery(Snapshot& s) {
    std::vector<std::vector<wmi::Row>> rows;
    if (!wmi::Query(L"SELECT EstimatedChargeRemaining,BatteryStatus FROM Win32_Battery", rows) ||
        rows.empty()) {
        s.hasBattery = false;
        return;
    }
    s.hasBattery = true;
    s.battery.chargePercent = _wtoi(RowVal(rows[0], L"EstimatedChargeRemaining").c_str());
    const int st = _wtoi(RowVal(rows[0], L"BatteryStatus").c_str());
    switch (st) {
    case 1: s.battery.status = L"__discharging__"; break;
    case 2: s.battery.status = L"__ac__"; break;
    case 3: s.battery.status = L"__charged__"; break;
    default: s.battery.status = L"—"; break;
    }
}

} // namespace

Snapshot Collect(CollectProgressFn onProgress) {
    Snapshot s;
    const auto step = [&](int idx) {
        if (onProgress) onProgress(idx, static_cast<float>(idx) / static_cast<float>(kCollectStepCount));
    };
    step(0);
    CollectWindows(s);
    step(1);
    CollectRegional(s);
    step(2);
    CollectSecurity(s);
    step(3);
    CollectCpu(s);
    step(4);
    CollectRam(s);
    step(5);
    CollectBoard(s);
    step(6);
    CollectGpus(s);
    step(7);
    CollectStorage(s);
    step(8);
    CollectNetwork(s);
    step(9);
    CollectBattery(s);
    if (onProgress) onProgress(kCollectStepCount - 1, 1.f);
    return s;
}

} // namespace maku::pci
