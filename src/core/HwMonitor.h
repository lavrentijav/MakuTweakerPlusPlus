#pragma once
#include <string>
#include <vector>

namespace maku::hwmon {

struct TempReading {
    std::wstring name;
    int celsius = -1;
};

struct SmartDisk {
    std::wstring name;
    std::wstring status;
    int healthPercent = -1;
};

struct Snapshot {
    std::vector<TempReading> temps;
    std::vector<SmartDisk> disks;
};

Snapshot Collect();

} // namespace maku::hwmon
