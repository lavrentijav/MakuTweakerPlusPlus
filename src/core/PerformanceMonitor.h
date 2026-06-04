#pragma once
#include <mutex>
#include <string>
#include <vector>

namespace maku::perfmon {

struct Snapshot {
    double cpuPercent = 0;
    double ramUsedPercent = 0;
    double ramUsedMb = 0;
    double ramTotalMb = 0;
    double diskReadMbps = 0;
    double diskWriteMbps = 0;
    double netDownMbps = 0;
    double netUpMbps = 0;
    std::wstring gpuName;
    double gpuUsagePercent = -1;
};

class PerformanceMonitor {
public:
    static PerformanceMonitor& Instance();

    void Start();
    void Stop();
    void Tick();
    Snapshot Get() const;

private:
    PerformanceMonitor() = default;
    mutable std::mutex mutex_;
    Snapshot snap_{};
    void* query_{nullptr};
    bool started_{false};
};

} // namespace maku::perfmon
