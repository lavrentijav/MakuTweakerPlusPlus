#pragma once
#include "core/CpuTopology.h"
#include "core/MetricsDb.h"
#include "core/MetricsSampler.h"

namespace maku::metrics {

/// Collects metrics in-process when the Windows service is not running.
class MetricsBackground {
public:
    static MetricsBackground& Instance();

    void Tick();
    MetricsDb& Db();
    const LiveSample& Live() const { return live_; }
    int64_t SampleKey() const { return sampleKey_; }
    cpu::CpuLayout& Layout() { return layout_; }
    bool DbReady() const { return dbReady_; }

private:
    void EnsureInit();
    void RefreshLiveFromDb();

    MetricsSampler sampler_;
    MetricsDb db_;
    cpu::CpuLayout layout_;
    LiveSample live_{};
    DWORD lastPollMs_ = 0;
    int64_t sampleKey_ = 0;
    bool samplerReady_ = false;
    bool dbReady_ = false;
    int samplerGen_ = 0;
};

} // namespace maku::metrics
