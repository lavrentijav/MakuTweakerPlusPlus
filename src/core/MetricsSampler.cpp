#include "core/MetricsSampler.h"
#include "core/CpuTopology.h"
#include <algorithm>
#include <ctime>
#include <pdh.h>
#include <windows.h>

#pragma comment(lib, "pdh.lib")

namespace maku::metrics {
namespace {

PDH_HQUERY AsQuery(void* q) { return static_cast<PDH_HQUERY>(q); }
PDH_HCOUNTER AsCounter(void* c) { return static_cast<PDH_HCOUNTER>(c); }

int ParseProcessorIndex(const wchar_t* path) {
    const wchar_t* open = wcschr(path, L'(');
    const wchar_t* close = open ? wcschr(open, L')') : nullptr;
    if (!open || !close || close <= open + 1) return -1;
    return _wtoi(open + 1);
}

void AddExpandedProcessorCounters(PDH_HQUERY q, std::vector<void*>& counters,
                                  std::vector<int>& procIndex) {
    DWORD pathChars = 0;
    PdhExpandCounterPathW(L"\\Processor(*)\\% Processor Time", nullptr, &pathChars);
    if (pathChars == 0) return;

    std::vector<wchar_t> paths(pathChars);
    if (PdhExpandCounterPathW(L"\\Processor(*)\\% Processor Time", paths.data(), &pathChars) !=
        ERROR_SUCCESS)
        return;

    for (wchar_t* path = paths.data(); *path; path += wcslen(path) + 1) {
        if (wcsstr(path, L"_Total")) continue;
        const int procIdx = ParseProcessorIndex(path);
        if (procIdx < 0) continue;
        PDH_HCOUNTER c = nullptr;
        if (PdhAddEnglishCounterW(q, path, 0, &c) != ERROR_SUCCESS) continue;
        counters.push_back(c);
        procIndex.push_back(procIdx);
    }
}

void AddFallbackProcessorCounters(PDH_HQUERY q, int logicalCount, std::vector<void*>& counters,
                                  std::vector<int>& procIndex) {
    counters.resize(static_cast<size_t>(logicalCount), nullptr);
    procIndex.resize(static_cast<size_t>(logicalCount));
    for (int i = 0; i < logicalCount; ++i) {
        wchar_t path[128];
        swprintf_s(path, L"\\Processor(%d)\\%% Processor Time", i);
        PDH_HCOUNTER c = nullptr;
        if (PdhAddEnglishCounterW(q, path, 0, &c) == ERROR_SUCCESS) {
            counters[static_cast<size_t>(i)] = c;
            procIndex[static_cast<size_t>(i)] = i;
        }
    }
}

} // namespace

MetricsSampler::MetricsSampler() = default;

MetricsSampler::~MetricsSampler() { Shutdown(); }

bool MetricsSampler::Init(const bool lightweight) {
    Shutdown();
    logicalCount_ = cpu::DetectLayout().logicalCount;
    perCore_ = true;
    lightweight_ = lightweight;
    serviceMode_ = false;
    return InitPdh();
}

bool MetricsSampler::InitForService() { return Init(true); }

void MetricsSampler::Shutdown() {
    if (query_) {
        PdhCloseQuery(AsQuery(query_));
        query_ = nullptr;
    }
    totalCpu_ = disk_ = gpu_ = net_ = nullptr;
    perCpu_.clear();
    perCpuProcIndex_.clear();
    pdhReady_ = false;
    perCore_ = true;
    lightweight_ = false;
    serviceMode_ = false;
    lastNetBytes_ = 0;
    lastNetTick_ = 0;
}

bool MetricsSampler::InitPdh() {
    PDH_HQUERY q = nullptr;
    if (PdhOpenQuery(nullptr, 0, &q) != ERROR_SUCCESS) return false;
    query_ = q;

    PDH_HCOUNTER c = nullptr;
    if (PdhAddEnglishCounterW(q, L"\\Processor(_Total)\\% Processor Time", 0, &c) == ERROR_SUCCESS)
        totalCpu_ = c;

    if (!serviceMode_) {
        AddExpandedProcessorCounters(q, perCpu_, perCpuProcIndex_);
        if (perCpu_.empty())
            AddFallbackProcessorCounters(q, logicalCount_, perCpu_, perCpuProcIndex_);

        c = nullptr;
        if (PdhAddEnglishCounterW(q, L"\\PhysicalDisk(_Total)\\% Disk Time", 0, &c) == ERROR_SUCCESS)
            disk_ = c;
    }

    if (!lightweight_ && !serviceMode_) {
        c = nullptr;
        if (PdhAddEnglishCounterW(q, L"\\GPU Engine(_Total)\\Utilization Percentage", 0, &c) ==
            ERROR_SUCCESS)
            gpu_ = c;

        c = nullptr;
        if (PdhAddEnglishCounterW(q, L"\\Network Interface(*)\\Bytes Total/sec", 0, &c) ==
            ERROR_SUCCESS)
            net_ = c;
    }

    PdhCollectQueryData(q);
    PdhCollectQueryData(q);
    pdhReady_ = true;
    return true;
}

bool MetricsSampler::Sample(LiveSample& out) {
    LiveSample s{};
    s.system.ts = static_cast<int64_t>(time(nullptr));

    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        s.system.ramPct = static_cast<float>(mem.dwMemoryLoad);
        s.system.ramUsed = static_cast<int64_t>(mem.ullTotalPhys - mem.ullAvailPhys);
    }

    s.perLogical.assign(serviceMode_ ? 0 : logicalCount_, 0.f);

    if (pdhReady_ && query_) {
        const auto q = AsQuery(query_);
        PdhCollectQueryData(q);
        PDH_FMT_COUNTERVALUE val{};

        auto read = [&](void* counter, float& dst) {
            if (!counter) return;
            if (PdhGetFormattedCounterValue(AsCounter(counter), PDH_FMT_DOUBLE, nullptr, &val) ==
                ERROR_SUCCESS)
                dst = static_cast<float>(std::clamp(val.doubleValue, 0.0, 100.0));
        };

        read(totalCpu_, s.system.cpuTotal);
        if (!serviceMode_) {
            for (size_t i = 0; i < perCpu_.size(); ++i) {
                float usage = 0.f;
                read(perCpu_[i], usage);
                const int idx =
                    i < perCpuProcIndex_.size() ? perCpuProcIndex_[i] : static_cast<int>(i);
                if (idx >= 0 && idx < logicalCount_)
                    s.perLogical[static_cast<size_t>(idx)] = usage;
            }
            read(disk_, s.system.diskPct);
            read(gpu_, s.system.gpuPct);

            if (net_ &&
                PdhGetFormattedCounterValue(AsCounter(net_), PDH_FMT_DOUBLE, nullptr, &val) ==
                    ERROR_SUCCESS) {
                const ULONGLONG bytes = static_cast<ULONGLONG>(val.doubleValue);
                const DWORD now = GetTickCount();
                if (lastNetTick_ != 0) {
                    const double dt = (now - lastNetTick_) / 1000.0;
                    if (dt > 0) s.system.netDown = static_cast<int64_t>((bytes - lastNetBytes_) / dt);
                }
                lastNetBytes_ = bytes;
                lastNetTick_ = now;
            }
        }
    }

    out = std::move(s);
    return true;
}

} // namespace maku::metrics
