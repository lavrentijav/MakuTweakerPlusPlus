#include "core/MetricsBackground.h"
#include "platform/MetricsService.h"
#include <ctime>
#include <windows.h>

namespace maku::metrics {

MetricsBackground& MetricsBackground::Instance() {
    static MetricsBackground bg;
    return bg;
}

void MetricsBackground::EnsureInit() {
    if (layout_.logicalCount == 0) layout_ = cpu::DetectLayout();
    if (!dbReady_) dbReady_ = db_.Open(db_.DefaultPath());
    static constexpr int kSamplerGen = 4;
    if (!samplerReady_ || samplerGen_ < kSamplerGen) {
        sampler_.Shutdown();
        samplerReady_ = sampler_.InitForGui();
        samplerGen_ = kSamplerGen;
    }
}

void MetricsBackground::RefreshLiveFromDb() {
    if (layout_.logicalCount == 0) layout_ = cpu::DetectLayout();
    const SystemSample latest = db_.QueryLatestSystem();
    if (latest.ts > 0) {
        if (latest.ts != live_.system.ts) ++sampleKey_;
        live_.system = latest;
    }
    if (layout_.logicalCount > 0) {
        const auto cpu = db_.QueryLatestCpuUsage(layout_.logicalCount);
        if (!cpu.empty()) live_.perLogical = cpu;
    }
}

MetricsDb& MetricsBackground::Db() {
    EnsureInit();
    return db_;
}

void MetricsBackground::Tick(const int intervalSec, const int uiRefreshMs) {
    EnsureInit();
    if (!dbReady_) return;

    const int periodSec = intervalSec > 0 ? intervalSec : 5;
    const bool svcRunning = metrics_svc::IsRunningCached();
    DWORD pollMs = svcRunning ? metrics_svc::kMetricsServicePollMs
                              : static_cast<DWORD>(std::max(1, periodSec) * 1000);
    if (uiRefreshMs > 0) {
        const DWORD uiCap = static_cast<DWORD>(std::max(1000, uiRefreshMs));
        pollMs = std::max(pollMs, uiCap);
    }
    const DWORD now = GetTickCount();
    if (lastPollMs_ != 0 && now - lastPollMs_ < pollMs) return;
    lastPollMs_ = now;

    if (svcRunning) {
        RefreshLiveFromDb();
        return;
    }

    if (!samplerReady_) return;

    LiveSample sample;
    if (!sampler_.Sample(sample)) return;
    live_ = std::move(sample);
    ++sampleKey_;
    db_.InsertSystemSample(live_.system);
    if (!live_.perLogical.empty())
        db_.InsertCpuSamples(live_.system.ts, live_.perLogical);
}

} // namespace maku::metrics
