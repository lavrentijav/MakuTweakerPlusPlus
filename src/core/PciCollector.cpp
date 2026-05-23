#include "core/PciCollector.h"

#include <chrono>

namespace maku::pci {

PciCollector& PciCollector::Instance() {
    static PciCollector c;
    return c;
}

PciCollector::PciCollector() { worker_ = std::thread([this] { WorkerLoop(); }); }

PciCollector::~PciCollector() { Shutdown(); }

void PciCollector::Shutdown() {
    stop_.store(true);
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
}

void PciCollector::Prefetch() { RequestCollect(false); }

void PciCollector::RequestCollect(bool force) {
    if (running_.load()) {
        if (force) rerun_.store(true);
        return;
    }
    if (ready_.load() && !force) return;
    if (!force && wake_.load()) return;

    {
        std::lock_guard lock(mutex_);
        if (force) ready_.store(false);
        else if (ready_.load()) return;
    }
    wake_.store(true);
    cv_.notify_one();
}

void PciCollector::Invalidate() {
    {
        std::lock_guard lock(mutex_);
        ready_.store(false);
    }
    RequestCollect(true);
}

bool PciCollector::IsReady() const { return ready_.load(); }

bool PciCollector::IsRunning() const { return running_.load(); }

int PciCollector::CurrentStep() const { return step_.load(); }

float PciCollector::Progress() const { return progress_.load(); }

bool PciCollector::TryGetSnapshot(Snapshot& out) {
    std::lock_guard lock(mutex_);
    if (!ready_) return false;
    out = snapshot_;
    return true;
}

void PciCollector::RunCollectPass() {
    running_.store(true);
    progress_.store(0.f);
    step_.store(0);

    Snapshot s = Collect([this](int step, float p) {
        step_.store(step);
        progress_.store(p);
    });

    {
        std::lock_guard lock(mutex_);
        snapshot_ = std::move(s);
        ready_.store(true);
    }
    running_.store(false);
}

void PciCollector::WorkerLoop() {
    while (!stop_.load()) {
        if (!wake_.exchange(false)) {
            std::unique_lock lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(100),
                         [this] { return stop_.load() || wake_.load(); });
            continue;
        }

        do {
            rerun_.store(false);
            RunCollectPass();
        } while (rerun_.load() && !stop_.load());
    }
}

} // namespace maku::pci
