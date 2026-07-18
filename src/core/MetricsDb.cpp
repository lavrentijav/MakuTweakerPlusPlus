#include "core/MetricsDb.h"
#include "core/StringUtil.h"
#include <sqlite3.h>
#include <algorithm>
#include <ctime>
#include <string>

namespace maku::metrics {
namespace {

constexpr int kRetentionDays = 30;
constexpr int kDownsampleAfterDays = 7;

} // namespace

MetricsDb::MetricsDb() = default;

MetricsDb::~MetricsDb() { Close(); }

std::wstring MetricsDb::DefaultPath() const {
    return util::GetSharedDataPath() + L"\\metrics.db";
}

bool MetricsDb::Open(const std::wstring& path, const bool serviceMode) {
    const std::wstring target = path.empty() ? DefaultPath() : path;
    if (GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const std::wstring legacy = util::GetAppDataPath() + L"\\metrics.db";
        if (GetFileAttributesW(legacy.c_str()) != INVALID_FILE_ATTRIBUTES)
            CopyFileW(legacy.c_str(), target.c_str(), FALSE);
    }
    Close();
    serviceMode_ = serviceMode;
    lastMaintenanceTs_ = 0;
    const std::string utf8 = util::ToUtf8(target);
    if (sqlite3_open(utf8.c_str(), &db_) != SQLITE_OK) {
        db_ = nullptr;
        return false;
    }
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA wal_autocheckpoint=2000;", nullptr, nullptr, nullptr);
    if (serviceMode_) {
        sqlite3_exec(db_, "PRAGMA cache_size=-512;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
        sqlite3_exec(db_, "PRAGMA mmap_size=0;", nullptr, nullptr, nullptr);
    }
    sqlite3_busy_timeout(db_, 2000);
    return EnsureSchema();
}

void MetricsDb::Close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
    serviceMode_ = false;
    lastMaintenanceTs_ = 0;
}

void MetricsDb::MaybeRunMaintenance(const int64_t ts) {
    if (lastMaintenanceTs_ != 0 && ts - lastMaintenanceTs_ < 3600) return;
    lastMaintenanceTs_ = ts;
    PurgeOlderThanDays(kRetentionDays);
    DownsampleCpuOlderThanDays(kDownsampleAfterDays);
}

void MetricsDb::RunMaintenance(const bool force) {
    if (!db_) return;
    const int64_t now = static_cast<int64_t>(time(nullptr));
    if (!force && lastMaintenanceTs_ != 0 && now - lastMaintenanceTs_ < 3600) return;
    lastMaintenanceTs_ = now;
    PurgeOlderThanDays(kRetentionDays);
    DownsampleCpuOlderThanDays(kDownsampleAfterDays);
    sqlite3_exec(db_, "PRAGMA wal_checkpoint(TRUNCATE);", nullptr, nullptr, nullptr);
    if (force) sqlite3_exec(db_, "VACUUM;", nullptr, nullptr, nullptr);
}

bool MetricsDb::EnsureSchema() {
    constexpr int kSchemaVersion = 2;

    const char* createSql = R"(
CREATE TABLE IF NOT EXISTS system_samples (
  ts INTEGER PRIMARY KEY,
  cpu_total REAL, ram_pct REAL, ram_used INTEGER,
  gpu_pct REAL, disk_pct REAL,
  net_down INTEGER, net_up INTEGER
);
CREATE TABLE IF NOT EXISTS cpu_samples (
  ts INTEGER NOT NULL,
  proc_index INTEGER NOT NULL,
  usage REAL NOT NULL,
  PRIMARY KEY (ts, proc_index)
);
CREATE INDEX IF NOT EXISTS idx_cpu_ts ON cpu_samples(ts);
CREATE INDEX IF NOT EXISTS idx_cpu_proc_ts ON cpu_samples(proc_index, ts);
CREATE INDEX IF NOT EXISTS idx_sys_ts ON system_samples(ts);
)";
    char* err = nullptr;
    if (sqlite3_exec(db_, createSql, nullptr, nullptr, &err) != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return false;
    }
    if (err) sqlite3_free(err);

    int currentVersion = 0;
    sqlite3_stmt* vstmt = nullptr;
    if (sqlite3_prepare_v2(db_, "PRAGMA user_version;", -1, &vstmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(vstmt) == SQLITE_ROW)
            currentVersion = sqlite3_column_int(vstmt, 0);
        sqlite3_finalize(vstmt);
    }

    if (currentVersion < kSchemaVersion) {
        // v1 -> v2: ensure idx_cpu_proc_ts and idx_sys_ts (already done above on legacy DBs).
        char buf[64];
        snprintf(buf, sizeof(buf), "PRAGMA user_version=%d;", kSchemaVersion);
        sqlite3_exec(db_, buf, nullptr, nullptr, nullptr);
    }

    return true;
}

bool MetricsDb::InsertSystemSample(const SystemSample& s) {
    if (!db_) return false;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "INSERT OR REPLACE INTO system_samples(ts,cpu_total,ram_pct,ram_used,gpu_pct,disk_pct,"
        "net_down,net_up) VALUES(?,?,?,?,?,?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, s.ts);
    sqlite3_bind_double(stmt, 2, s.cpuTotal);
    sqlite3_bind_double(stmt, 3, s.ramPct);
    sqlite3_bind_int64(stmt, 4, s.ramUsed);
    sqlite3_bind_double(stmt, 5, s.gpuPct);
    sqlite3_bind_double(stmt, 6, s.diskPct);
    sqlite3_bind_int64(stmt, 7, s.netDown);
    sqlite3_bind_int64(stmt, 8, s.netUp);
    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    MaybeRunMaintenance(s.ts);
    return ok;
}

bool MetricsDb::InsertCpuSamples(int64_t ts, const std::vector<float>& perLogical) {
    if (!db_ || perLogical.empty()) return false;
    sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);
    sqlite3_stmt* stmt = nullptr;
    const char* sql = "INSERT OR REPLACE INTO cpu_samples(ts,proc_index,usage) VALUES(?,?,?)";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
        return false;
    }
    for (int i = 0; i < static_cast<int>(perLogical.size()); ++i) {
        sqlite3_bind_int64(stmt, 1, ts);
        sqlite3_bind_int(stmt, 2, i);
        sqlite3_bind_double(stmt, 3, perLogical[i]);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
            return false;
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);
    return true;
}

bool MetricsDb::InsertBatch(const std::vector<SystemSample>& system,
                            const std::vector<CpuSampleRow>& cpu) {
    if (!db_) return false;
    if (system.empty() && cpu.empty()) return true;

    sqlite3_exec(db_, "BEGIN IMMEDIATE;", nullptr, nullptr, nullptr);

    if (!system.empty()) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "INSERT OR REPLACE INTO system_samples(ts,cpu_total,ram_pct,ram_used,gpu_pct,disk_pct,"
            "net_down,net_up) VALUES(?,?,?,?,?,?,?,?)";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            for (const auto& s : system) {
                sqlite3_bind_int64(stmt, 1, s.ts);
                sqlite3_bind_double(stmt, 2, s.cpuTotal);
                sqlite3_bind_double(stmt, 3, s.ramPct);
                sqlite3_bind_int64(stmt, 4, s.ramUsed);
                sqlite3_bind_double(stmt, 5, s.gpuPct);
                sqlite3_bind_double(stmt, 6, s.diskPct);
                sqlite3_bind_int64(stmt, 7, s.netDown);
                sqlite3_bind_int64(stmt, 8, s.netUp);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }
    }

    if (!cpu.empty()) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT OR REPLACE INTO cpu_samples(ts,proc_index,usage) VALUES(?,?,?)";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            for (const auto& row : cpu) {
                sqlite3_bind_int64(stmt, 1, row.ts);
                sqlite3_bind_int(stmt, 2, row.procIndex);
                sqlite3_bind_double(stmt, 3, row.usage);
                sqlite3_step(stmt);
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);
        }
    }

    sqlite3_exec(db_, "COMMIT;", nullptr, nullptr, nullptr);

    const int64_t maintTs = !system.empty() ? system.back().ts
                          : !cpu.empty()    ? cpu.back().ts
                                              : 0;
    if (maintTs > 0) MaybeRunMaintenance(maintTs);
    return true;
}

void MetricsDb::PurgeOlderThanDays(int days) {
    if (!db_) return;
    const int64_t cutoff = static_cast<int64_t>(time(nullptr)) - static_cast<int64_t>(days) * 86400;
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM system_samples WHERE ts < ?", -1, &stmt, nullptr) ==
        SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    if (sqlite3_prepare_v2(db_, "DELETE FROM cpu_samples WHERE ts < ?", -1, &stmt, nullptr) ==
        SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void MetricsDb::DownsampleCpuOlderThanDays(int days) {
    if (!db_) return;
    const int64_t cutoff = static_cast<int64_t>(time(nullptr)) - static_cast<int64_t>(days) * 86400;
    const char* sql = R"(
DELETE FROM cpu_samples WHERE ts < ? AND ts NOT IN (
  SELECT ts FROM cpu_samples WHERE ts < ?
  GROUP BY (ts / 60), proc_index
);
)";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, cutoff);
        sqlite3_bind_int64(stmt, 2, cutoff);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

std::vector<SystemSample> MetricsDb::QuerySystem(int64_t sinceTs, int64_t untilTs,
                                                 int maxRows) const {
    std::vector<SystemSample> out;
    if (!db_ || maxRows <= 0) return out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql = untilTs > 0
                          ? "SELECT ts,cpu_total,ram_pct,ram_used,gpu_pct,disk_pct,net_down,net_up "
                            "FROM system_samples WHERE ts>=? AND ts<=? ORDER BY ts LIMIT ?"
                          : "SELECT ts,cpu_total,ram_pct,ram_used,gpu_pct,disk_pct,net_down,net_up "
                            "FROM system_samples WHERE ts>=? ORDER BY ts LIMIT ?";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    sqlite3_bind_int64(stmt, 1, sinceTs);
    if (untilTs > 0) {
        sqlite3_bind_int64(stmt, 2, untilTs);
        sqlite3_bind_int(stmt, 3, maxRows);
    } else {
        sqlite3_bind_int(stmt, 2, maxRows);
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SystemSample s{};
        s.ts = sqlite3_column_int64(stmt, 0);
        s.cpuTotal = static_cast<float>(sqlite3_column_double(stmt, 1));
        s.ramPct = static_cast<float>(sqlite3_column_double(stmt, 2));
        s.ramUsed = sqlite3_column_int64(stmt, 3);
        s.gpuPct = static_cast<float>(sqlite3_column_double(stmt, 4));
        s.diskPct = static_cast<float>(sqlite3_column_double(stmt, 5));
        s.netDown = sqlite3_column_int64(stmt, 6);
        s.netUp = sqlite3_column_int64(stmt, 7);
        out.push_back(s);
    }
    sqlite3_finalize(stmt);
    return out;
}

SystemSample MetricsDb::QueryLatestSystem() const {
    SystemSample s{};
    if (!db_) return s;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT ts,cpu_total,ram_pct,ram_used,gpu_pct,disk_pct,net_down,net_up "
        "FROM system_samples ORDER BY ts DESC LIMIT 1";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return s;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        s.ts = sqlite3_column_int64(stmt, 0);
        s.cpuTotal = static_cast<float>(sqlite3_column_double(stmt, 1));
        s.ramPct = static_cast<float>(sqlite3_column_double(stmt, 2));
        s.ramUsed = sqlite3_column_int64(stmt, 3);
        s.gpuPct = static_cast<float>(sqlite3_column_double(stmt, 4));
        s.diskPct = static_cast<float>(sqlite3_column_double(stmt, 5));
        s.netDown = sqlite3_column_int64(stmt, 6);
        s.netUp = sqlite3_column_int64(stmt, 7);
    }
    sqlite3_finalize(stmt);
    return s;
}

std::vector<CpuSampleRow> MetricsDb::QueryCpu(int64_t sinceTs, int procIndex, int64_t untilTs,
                                              int maxRows) const {
    std::vector<CpuSampleRow> out;
    if (!db_) return out;
    out.reserve(maxRows > 0 ? static_cast<size_t>(std::min(maxRows, 65536)) : 8192);
    sqlite3_stmt* stmt = nullptr;
    const char* limitClause = maxRows > 0 ? " LIMIT ?" : "";
    const char* sqlProc = untilTs > 0
                              ? "SELECT ts,proc_index,usage FROM cpu_samples WHERE ts>=? AND "
                                "ts<=? AND proc_index=? ORDER BY ts DESC"
                              : "SELECT ts,proc_index,usage FROM cpu_samples WHERE ts>=? AND "
                                "proc_index=? ORDER BY ts DESC";
    const char* sqlAll = untilTs > 0
                             ? "SELECT ts,proc_index,usage FROM cpu_samples WHERE ts>=? AND "
                               "ts<=? ORDER BY ts DESC,proc_index"
                             : "SELECT ts,proc_index,usage FROM cpu_samples WHERE ts>=? ORDER BY "
                               "ts DESC,proc_index";
    std::string sql = procIndex >= 0 ? sqlProc : sqlAll;
    if (maxRows > 0) sql += limitClause;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) return out;
    int bind = 1;
    sqlite3_bind_int64(stmt, bind++, sinceTs);
    if (untilTs > 0) sqlite3_bind_int64(stmt, bind++, untilTs);
    if (procIndex >= 0) sqlite3_bind_int(stmt, bind++, procIndex);
    if (maxRows > 0) sqlite3_bind_int(stmt, bind++, maxRows);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        CpuSampleRow r{};
        r.ts = sqlite3_column_int64(stmt, 0);
        r.procIndex = sqlite3_column_int(stmt, 1);
        r.usage = static_cast<float>(sqlite3_column_double(stmt, 2));
        out.push_back(r);
    }
    sqlite3_finalize(stmt);
    if (maxRows > 0 && out.size() > 1)
        std::reverse(out.begin(), out.end());
    return out;
}

std::vector<CpuSampleRow> MetricsDb::QueryCpuLatest(int64_t ts) const {
    return QueryCpu(ts, -1, ts);
}

std::vector<float> MetricsDb::QueryLatestCpuUsage(int maxLogical) const {
    std::vector<float> out(static_cast<size_t>(maxLogical), 0.f);
    if (!db_ || maxLogical <= 0) return out;
    sqlite3_stmt* stmt = nullptr;
    const char* sql =
        "SELECT proc_index, usage FROM cpu_samples WHERE ts=(SELECT MAX(ts) FROM cpu_samples) "
        "ORDER BY proc_index";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return out;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const int idx = sqlite3_column_int(stmt, 0);
        if (idx >= 0 && idx < maxLogical)
            out[static_cast<size_t>(idx)] = static_cast<float>(sqlite3_column_double(stmt, 1));
    }
    sqlite3_finalize(stmt);
    return out;
}

} // namespace maku::metrics
