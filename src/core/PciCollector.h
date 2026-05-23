#pragma once

#include "core/SysInfo.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace maku::pci {

/// Background PC info snapshot (WMI / registry). Safe to call from UI thread.
class PciCollector {
public:
    static PciCollector& Instance();

    PciCollector(const PciCollector&) = delete;
    PciCollector& operator=(const PciCollector&) = delete;

    /// Start collection if idle; no-op while already running or ready (unless force).
    void RequestCollect(bool force = false);

    /// Mark cached data stale and schedule a new background run.
    void Invalidate();

    /// Start prefetch on app launch (same as RequestCollect(false)).
    void Prefetch();

    bool IsReady() const;
    bool IsRunning() const;

    int CurrentStep() const;
    float Progress() const;

    /// Copy cached snapshot when ready.
    bool TryGetSnapshot(Snapshot& out);

    void Shutdown();

private:
    PciCollector();
    ~PciCollector();

    void WorkerLoop();
    void RunCollectPass();

    mutable std::mutex mutex_;
    std::thread worker_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
    std::atomic<bool> wake_{false};
    std::atomic<bool> running_{false};
    std::atomic<bool> ready_{false};
    std::atomic<bool> rerun_{false};
    std::atomic<int> step_{0};
    std::atomic<float> progress_{0.f};
    Snapshot snapshot_;
};

} // namespace maku::pci
