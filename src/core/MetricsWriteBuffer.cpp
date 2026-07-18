#include "core/MetricsWriteBuffer.h"

namespace maku::metrics {

void MetricsWriteBuffer::Reset(const DWORD tick) {
    system_.clear();
    cpu_.clear();
    system_.shrink_to_fit();
    cpu_.shrink_to_fit();
    lastFlushTick_ = tick;
}

size_t MetricsWriteBuffer::EstimatedBytes() const {
    return system_.size() * sizeof(SystemSample) + cpu_.size() * sizeof(CpuSampleRow);
}

void MetricsWriteBuffer::PushSystem(const SystemSample& s) { system_.push_back(s); }

void MetricsWriteBuffer::PushCpu(const int64_t ts, const std::vector<float>& perLogical) {
    if (perLogical.empty()) return;
    cpu_.reserve(cpu_.size() + perLogical.size());
    for (int i = 0; i < static_cast<int>(perLogical.size()); ++i) {
        cpu_.push_back(CpuSampleRow{ts, i, perLogical[static_cast<size_t>(i)]});
    }
}

bool MetricsWriteBuffer::ShouldFlush(const DWORD tick) const {
    if (Empty()) return false;
    return tick - lastFlushTick_ >= kFlushIntervalMs;
}

bool MetricsWriteBuffer::IsFull() const { return EstimatedBytes() >= kMaxBytes; }

void MetricsWriteBuffer::Flush(MetricsDb& db) {
    if (Empty()) return;
    db.InsertBatch(system_, cpu_);
    system_.clear();
    cpu_.clear();
    lastFlushTick_ = GetTickCount();
}

} // namespace maku::metrics
