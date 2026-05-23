#pragma once
#include <functional>
#include <string>
#include <vector>

namespace maku::pci {

struct GpuInfo {
    std::wstring name;
    unsigned long long vramBytes = 0;
    std::wstring driverVersion;
};

struct DiskInfo {
    std::wstring model;
    unsigned long long sizeBytes = 0;
    std::wstring mediaType;
    std::wstring interfaceType;
};

struct LogicalDriveInfo {
    std::wstring id;
    std::wstring fileSystem;
    unsigned long long sizeBytes = 0;
    unsigned long long freeBytes = 0;
};

struct RamStickInfo {
    std::wstring manufacturer;
    unsigned long long capacityBytes = 0;
    int speedMhz = 0;
    std::wstring partNumber;
};

struct NetAdapterInfo {
    std::wstring description;
    std::wstring mac;
    std::wstring ipv4;
    std::wstring ipv6;
};

struct BatteryInfo {
    int chargePercent = -1;
    std::wstring status;
};

struct Snapshot {
    // Windows
    std::wstring osCaption;
    std::wstring displayVersion;
    std::wstring edition;
    std::wstring build;
    std::wstring arch;
    std::wstring installDate;
    std::wstring activation;
    std::wstring computerName;
    std::wstring username;

    // Regional
    std::wstring uiLanguage;
    std::wstring region;
    std::wstring timezone;

    // Virtualization / security
    std::wstring wdac;
    std::wstring vbs;
    std::wstring hyperv;
    std::wstring defender;
    std::wstring coreIsolation;
    std::wstring uac;
    std::wstring smartScreen;
    std::wstring tpm;
    std::wstring secureBoot;

    // Hardware
    std::wstring cpuName;
    int cpuCores = 0;
    int cpuThreads = 0;
    double cpuBaseGhz = 0;
    int cpuL3Mb = 0;
    std::wstring cpuArch;

    unsigned long long ramTotalBytes = 0;
    std::wstring ramType;
    int ramSpeedMhz = 0;

    std::wstring motherboard;
    std::wstring biosVersion;
    std::wstring biosDate;
    std::wstring pcManufacturer;
    std::wstring pcModel;

    std::vector<GpuInfo> gpus;
    std::vector<DiskInfo> disks;
    std::vector<LogicalDriveInfo> volumes;
    std::vector<RamStickInfo> ramSticks;
    std::vector<NetAdapterInfo> netAdapters;
    bool hasBattery = false;
    BatteryInfo battery;
};

/// step: 0..9 (see pci::kCollectStepCount), progress: 0..1
using CollectProgressFn = std::function<void(int step, float progress)>;

constexpr int kCollectStepCount = 10;

Snapshot Collect(CollectProgressFn onProgress = nullptr);
std::wstring FormatBytes(unsigned long long bytes);

} // namespace maku::pci
