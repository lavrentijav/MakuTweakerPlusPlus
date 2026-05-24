#pragma once
#include "core/CpuTopology.h"
#include "core/Localization.h"
#include "core/MetricsDb.h"
#include "ui/MonitorPlotAxis.h"
#include <vector>

namespace maku::ui {

enum class CpuViewMode { Auto, Threads, Cores, Numa, Heatmap, TopN };
enum class CpuUnitMode { Logical, Physical };

struct CpuVizState {
    CpuViewMode viewMode = CpuViewMode::Auto;
    CpuUnitMode unitMode = CpuUnitMode::Logical;
    int topN = 5;
    bool manualOverride = false;
};

CpuViewMode ResolveViewMode(const CpuVizState& state, const cpu::CpuLayout& layout,
                            CpuUnitMode unit, const std::vector<float>& usage);

void DrawCpuVisualizer(const l10n::Localization& l, const cpu::CpuLayout& layout,
                       const std::vector<float>& usage, CpuVizState& state, int64_t liveSampleKey,
                       double timeUnixSec, const char* plotId, int visibleSeconds, int rangeHours,
                       MonitorTimeAxisView& timeView,
                       const std::vector<metrics::CpuSampleRow>& cpuHistory);

} // namespace maku::ui
