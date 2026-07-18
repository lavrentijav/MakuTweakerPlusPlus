#include "core/PerformanceMonitor.h"
#include "core/Wmi.h"
#include <pdh.h>
#include <psapi.h>
#pragma comment(lib, "pdh.lib")

namespace maku::perfmon {
namespace {

bool AddCounter(PDH_HQUERY q, const wchar_t* path, std::vector<void*>& out) {
    HCOUNTER c{};
    if (PdhAddCounterW(q, path, 0, &c) != ERROR_SUCCESS) return false;
    out.push_back(c);
    PdhCollectQueryData(q);
    return true;
}

double ReadCounter(HCOUNTER c) {
    PDH_FMT_COUNTERVALUE val{};
    if (PdhGetFormattedCounterValue(c, PDH_FMT_DOUBLE, nullptr, &val) != ERROR_SUCCESS)
        return 0;
    return val.doubleValue;
}

} // namespace

PerformanceMonitor& PerformanceMonitor::Instance() {
    static PerformanceMonitor m;
    return m;
}

void PerformanceMonitor::Start() {
    if (started_) return;
    PDH_HQUERY q{};
    if (PdhOpenQuery(nullptr, 0, &q) != ERROR_SUCCESS) return;
    query_ = q;
    counters_.clear();
    AddCounter(q, L"\\Processor(_Total)\\% Processor Time", counters_);
    AddCounter(q, L"\\PhysicalDisk(_Total)\\Disk Read Bytes/sec", counters_);
    AddCounter(q, L"\\PhysicalDisk(_Total)\\Disk Write Bytes/sec", counters_);
    snap_.gpuName = wmi::QueryScalar(L"SELECT Name FROM Win32_VideoController", L"Name");
    started_ = true;
    Tick();
    Tick();
}

void PerformanceMonitor::Stop() {
    if (!query_) return;
    for (void* c : counters_) PdhRemoveCounter(reinterpret_cast<HCOUNTER>(c));
    counters_.clear();
    PdhCloseQuery(reinterpret_cast<PDH_HQUERY>(query_));
    query_ = nullptr;
    started_ = false;
}

void PerformanceMonitor::Tick() {
    if (!query_ || counters_.size() < 3) return;
    auto* q = reinterpret_cast<PDH_HQUERY>(query_);
    PdhCollectQueryData(q);

    Snapshot s;
    s.cpuPercent = ReadCounter(reinterpret_cast<HCOUNTER>(counters_[0]));

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        s.ramTotalMb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0);
        s.ramUsedMb = static_cast<double>(mem.ullTotalPhys - mem.ullAvailPhys) / (1024.0 * 1024.0);
        s.ramUsedPercent = mem.dwMemoryLoad;
    }

    const double readB = ReadCounter(reinterpret_cast<HCOUNTER>(counters_[1]));
    const double writeB = ReadCounter(reinterpret_cast<HCOUNTER>(counters_[2]));
    s.diskReadMbps = readB / (1024.0 * 1024.0);
    s.diskWriteMbps = writeB / (1024.0 * 1024.0);
    s.gpuName = snap_.gpuName;

    std::lock_guard lock(mutex_);
    snap_ = s;
}

Snapshot PerformanceMonitor::Get() const {
    std::lock_guard lock(mutex_);
    return snap_;
}

} // namespace maku::perfmon
