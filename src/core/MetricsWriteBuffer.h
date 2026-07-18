#pragma once
#include "core/MetricsDb.h"
#include <cstddef>
#include <vector>
#include <windows.h>

namespace maku::metrics {

/// In-memory write-ahead buffer for the metrics service (flush to SQLite periodically).
class MetricsWriteBuffer {
public:
    static constexpr size_t kMaxBytes = 2u * 1024u * 1024u;
    static constexpr DWORD kFlushIntervalMs = 10000;

    void Reset(DWORD tick);
    void PushSystem(const SystemSample& s);
    void PushCpu(int64_t ts, const std::vector<float>& perLogical);
    bool ShouldFlush(DWORD tick) const;
    bool IsFull() const;
    bool Empty() const { return system_.empty() && cpu_.empty(); }
    void Flush(MetricsDb& db);

private:
    size_t EstimatedBytes() const;

    std::vector<SystemSample> system_;
    std::vector<CpuSampleRow> cpu_;
    DWORD lastFlushTick_ = 0;
};

} // namespace maku::metrics
