#pragma once
#include "core/MetricsDb.h"
#include <vector>
#include <windows.h>

namespace maku::metrics {

struct LiveSample {
    SystemSample system;
    std::vector<float> perLogical;
};

class MetricsSampler {
public:
    MetricsSampler();
    ~MetricsSampler();

    /// @param lightweight If true, skip GPU/network PDH counters (per-core CPU is always collected).
    bool Init(bool lightweight = false);
    /// Minimal PDH footprint for the Windows metrics service (total CPU + RAM only).
    bool InitForService();
    void Shutdown();
    bool Sample(LiveSample& out);

private:
    bool InitPdh();

    void* query_ = nullptr;
    void* totalCpu_ = nullptr;
    std::vector<void*> perCpu_;
    std::vector<int> perCpuProcIndex_;
    void* disk_ = nullptr;
    void* gpu_ = nullptr;
    void* net_ = nullptr;
    int logicalCount_ = 0;
    bool pdhReady_ = false;
    bool perCore_ = true;
    bool lightweight_ = false;
    bool serviceMode_ = false;
    ULONGLONG lastNetBytes_ = 0;
    DWORD lastNetTick_ = 0;
};

} // namespace maku::metrics
