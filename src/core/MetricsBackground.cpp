#include "core/MetricsBackground.h"
#include "core/Settings.h"
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
    static constexpr int kSamplerGen = 2;
    if (!samplerReady_ || samplerGen_ < kSamplerGen) {
        sampler_.Shutdown();
        samplerReady_ = sampler_.Init(true);
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

void MetricsBackground::Tick() {
    EnsureInit();
    if (!dbReady_) return;

    Settings settings;
    settings.Load();
    const int intervalSec = settings.metricsIntervalSec > 0 ? settings.metricsIntervalSec : 5;
    const bool svcRunning = metrics_svc::IsRunningCached();
    const DWORD pollMs = svcRunning ? metrics_svc::kMetricsServicePollMs
                                    : static_cast<DWORD>(std::max(1, intervalSec) * 1000);
    const DWORD now = GetTickCount();
    if (lastPollMs_ != 0 && now - lastPollMs_ < pollMs) return;
    lastPollMs_ = now;

    if (svcRunning) {
        // Service owns persistence; UI samples locally only for the live tile/spark, then
        // reads recent history straight from the DB. The local sample is discarded after
        // populating `live_` to avoid double counters and double DB writes.
        LiveSample sample;
        if (samplerReady_ && sampler_.Sample(sample)) {
            live_ = std::move(sample);
            ++sampleKey_;
        } else {
            RefreshLiveFromDb();
        }
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
