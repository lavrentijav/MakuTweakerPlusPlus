#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct sqlite3;

namespace maku::metrics {

struct SystemSample {
    int64_t ts = 0;
    float cpuTotal = 0.f;
    float ramPct = 0.f;
    int64_t ramUsed = 0;
    float gpuPct = 0.f;
    float diskPct = 0.f;
    int64_t netDown = 0;
    int64_t netUp = 0;
};

struct CpuSampleRow {
    int64_t ts = 0;
    int procIndex = 0;
    float usage = 0.f;
};

class MetricsDb {
public:
    MetricsDb();
    ~MetricsDb();

    MetricsDb(const MetricsDb&) = delete;
    MetricsDb& operator=(const MetricsDb&) = delete;

    bool Open(const std::wstring& path, bool serviceMode = false);
    void Close();
    bool IsOpen() const { return db_ != nullptr; }

    std::wstring DefaultPath() const;

    bool InsertSystemSample(const SystemSample& s);
    bool InsertCpuSamples(int64_t ts, const std::vector<float>& perLogical);
    bool InsertBatch(const std::vector<SystemSample>& system, const std::vector<CpuSampleRow>& cpu);

    void PurgeOlderThanDays(int days);
    void DownsampleCpuOlderThanDays(int days);

    /// Run housekeeping (purge old data, downsample, WAL checkpoint, optional VACUUM).
    /// @param force If true, run regardless of the last-maintenance throttle and include VACUUM.
    void RunMaintenance(bool force = false);

    std::vector<SystemSample> QuerySystem(int64_t sinceTs, int64_t untilTs = 0,
                                          int maxRows = 4096) const;
    SystemSample QueryLatestSystem() const;
    std::vector<CpuSampleRow> QueryCpu(int64_t sinceTs, int procIndex = -1,
                                       int64_t untilTs = 0) const;
    std::vector<CpuSampleRow> QueryCpuLatest(int64_t ts) const;
    /// Latest per-logical usage snapshot (one row per proc_index).
    std::vector<float> QueryLatestCpuUsage(int maxLogical) const;

private:
    bool EnsureSchema();
    void MaybeRunMaintenance(int64_t ts);

    sqlite3* db_ = nullptr;
    bool serviceMode_ = false;
    int64_t lastMaintenanceTs_ = 0;
};

} // namespace maku::metrics
