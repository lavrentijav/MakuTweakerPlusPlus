#include "core/PerformanceMonitor.h"
#include "core/Wmi.h"
#include <pdh.h>
#include <psapi.h>
#pragma comment(lib, "pdh.lib")

namespace maku::perfmon {

PerformanceMonitor& PerformanceMonitor::Instance() {
    static PerformanceMonitor m;
    return m;
}

void PerformanceMonitor::Start() {
    if (started_) return;
    PdhOpenQuery(nullptr, 0, reinterpret_cast<PDH_HQUERY*>(&query_));
    auto add = [&](const wchar_t* path) {
        HCOUNTER c{};
        if (PdhAddCounterW(reinterpret_cast<PDH_HQUERY>(query_), path, 0, &c) == ERROR_SUCCESS)
            PdhCollectQueryData(reinterpret_cast<PDH_HQUERY>(query_));
    };
    add(L"\\Processor(_Total)\\% Processor Time");
    add(L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec");
    add(L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec");
    started_ = true;
    Tick();
}

void PerformanceMonitor::Stop() {
    if (query_) {
        PdhCloseQuery(reinterpret_cast<PDH_HQUERY>(query_));
        query_ = nullptr;
    }
    started_ = false;
}

void PerformanceMonitor::Tick() {
    if (!query_) return;
    PdhCollectQueryData(reinterpret_cast<PDH_HQUERY>(query_));

    auto readCounter = [&](const wchar_t* path, double& out) {
        HCOUNTER c{};
        if (PdhAddCounterW(reinterpret_cast<PDH_HQUERY>(query_), path, 0, &c) != ERROR_SUCCESS) return;
        PDH_FMT_COUNTERVALUE val{};
        if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &val) == ERROR_SUCCESS)
            out = val.doubleValue;
        PdhRemoveCounter(c);
    };

    Snapshot s;
    readCounter(L"\\Processor(_Total)\\% Processor Time", s.cpuPercent);

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        s.ramTotalMb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0);
        s.ramUsedMb = static_cast<double>(mem.ullTotalPhys - mem.ullAvailPhys) / (1024.0 * 1024.0);
        s.ramUsedPercent = mem.dwMemoryLoad;
    }

    double readB = 0, writeB = 0;
    readCounter(L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", readB);
    readCounter(L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", writeB);
    s.diskReadMbps = readB / (1024.0 * 1024.0);
    s.diskWriteMbps = writeB / (1024.0 * 1024.0);

    s.gpuName = wmi::QueryScalar(L"SELECT Name FROM Win32_VideoController", L"Name");

    std::lock_guard lock(mutex_);
    snap_ = s;
}

Snapshot PerformanceMonitor::Get() const {
    std::lock_guard lock(mutex_);
    return snap_;
}

} // namespace maku::perfmon
