#include "ui/CpuVisualizer.h"
#include "ui/MonitorPlotAxis.h"
#include "ui/Theme.h"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <string>
#include <vector>

namespace maku::ui {
namespace {

constexpr int kAutoLinearMax = 24;  // below: all-units line chart
constexpr int kAutoNumaMax = 128;   // 24..128: NUMA line charts; above: heatmap

double NowSec() { return static_cast<double>(time(nullptr)); }

double NormalizeTimeSec(double timeSec) { return timeSec > 0.0 ? timeSec : NowSec(); }

std::vector<float> ResolveUsage(const cpu::CpuLayout& layout, const std::vector<float>& logical,
                                CpuUnitMode unit) {
    if (unit == CpuUnitMode::Physical) return cpu::AggregateToCores(layout, logical);
    return logical;
}

ImU32 HeatColor(float pct) {
    pct = std::clamp(pct, 0.f, 100.f) / 100.f;
    ImVec4 low{0.16f, 0.16f, 0.16f, 1.f};
    ImVec4 mid{0.13f, 0.77f, 0.37f, 1.f};
    ImVec4 hi{0.94f, 0.27f, 0.27f, 1.f};
    ImVec4 c = pct < 0.5f ? ImVec4(low.x + (mid.x - low.x) * pct * 2.f, low.y + (mid.y - low.y) * pct * 2.f,
                                   low.z + (mid.z - low.z) * pct * 2.f, 1.f)
                            : ImVec4(mid.x + (hi.x - mid.x) * (pct - 0.5f) * 2.f,
                                     mid.y + (hi.y - mid.y) * (pct - 0.5f) * 2.f,
                                     mid.z + (hi.z - mid.z) * (pct - 0.5f) * 2.f, 1.f);
    return ImGui::ColorConvertFloat4ToU32(c);
}

void SetupPercentTimeAxes(const l10n::Localization& l, MonitorTimeAxisView& timeView,
                          double dataXMin, double dataXMax, int visibleSeconds, int rangeHours) {
    ImPlot::SetupAxes(l.Get("mon", "graph", "time").c_str(), l.Get("mon", "graph", "pct").c_str());
    ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
    ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImPlotCond_Always);
    ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, 100.0);
    SetupMonitorTimeAxisX(timeView, dataXMin, dataXMax, visibleSeconds, rangeHours);
    ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Outside);
}

std::string SeriesLabel(const l10n::Localization& l, CpuUnitMode unit, int id) {
    const char* prefix =
        unit == CpuUnitMode::Physical ? "core_lbl" : "thread_lbl";
    return l.Get("mon", "cpu", prefix) + std::to_string(id);
}

struct CpuTimelineData {
    std::vector<double> times;
    std::vector<std::vector<double>> series;
    int seriesCount = 0;
};

void CpuPlotTimeBounds(const CpuTimelineData& data, double timeEndSec, int visibleSeconds,
                       double& outXMin, double& outXMax) {
    const double end = NormalizeTimeSec(timeEndSec);
    const int span = std::max(30, visibleSeconds);
    outXMax = end;
    outXMin = end - static_cast<double>(span);
    if (!data.times.empty()) {
        outXMax = std::max(outXMax, data.times.back());
        outXMin = std::min(data.times.front(), outXMax - static_cast<double>(span));
    }
}

CpuTimelineData BuildTimeline(const std::vector<metrics::CpuSampleRow>& rows,
                              const cpu::CpuLayout& layout, CpuUnitMode unit, int seriesCount,
                              const std::vector<float>& liveUsage, int64_t liveTs) {
    std::map<int64_t, std::vector<float>> byTs;
    for (const auto& r : rows) {
        auto& snap = byTs[r.ts];
        if (r.procIndex >= static_cast<int>(snap.size()))
            snap.resize(static_cast<size_t>(r.procIndex + 1), 0.f);
        snap[static_cast<size_t>(r.procIndex)] = r.usage;
    }
    if (liveTs > 0 && !liveUsage.empty()) byTs[liveTs] = liveUsage;

    CpuTimelineData out;
    if (seriesCount <= 0) return out;
    out.seriesCount = seriesCount;
    out.series.assign(static_cast<size_t>(seriesCount), {});

    for (const auto& kv : byTs) {
        out.times.push_back(static_cast<double>(kv.first));
        std::vector<float> resolved = ResolveUsage(layout, kv.second, unit);
        if (static_cast<int>(resolved.size()) < seriesCount)
            resolved.resize(static_cast<size_t>(seriesCount), 0.f);
        for (int i = 0; i < seriesCount; ++i)
            out.series[static_cast<size_t>(i)].push_back(
                static_cast<double>(std::clamp(resolved[static_cast<size_t>(i)], 0.f, 100.f)));
    }
    return out;
}

void PlotTimelineSeries(const l10n::Localization& l, const CpuTimelineData& data, CpuUnitMode unit,
                        const std::vector<int>* onlyIndices) {
    if (data.times.empty()) return;

    const int budget = PlotDownsampleBudget();
    const ImPlotRect lim = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
    static thread_local std::vector<double> filteredX, filteredY, plotX, plotY;

    const auto plotOne = [&](int id) {
        if (id < 0 || id >= data.seriesCount) return;
        const auto& ysFull = data.series[static_cast<size_t>(id)];
        if (ysFull.size() != data.times.size()) return;
        FilterSeriesByTime(data.times, ysFull, lim.X.Min, lim.X.Max, filteredX, filteredY);
        if (filteredX.empty()) return;
        DownsampleAverage(filteredX, filteredY, plotX, plotY, budget);
        if (plotX.empty()) return;
        ImPlot::PlotLine(SeriesLabel(l, unit, id).c_str(), plotX.data(), plotY.data(),
                         static_cast<int>(plotX.size()));
    };
    if (onlyIndices) {
        for (int id : *onlyIndices) plotOne(id);
    } else {
        for (int i = 0; i < data.seriesCount; ++i) plotOne(i);
    }
}

float PlotHeight() {
    return std::max(220.f, ImGui::GetContentRegionAvail().y - 4.f);
}

int UnitCount(const cpu::CpuLayout& layout, CpuUnitMode unit) {
    return unit == CpuUnitMode::Physical ? layout.coreCount : layout.logicalCount;
}

int ActiveUnitCount(const cpu::CpuLayout& layout, CpuUnitMode unit,
                    const std::vector<float>& usage) {
    if (unit == CpuUnitMode::Physical)
        return layout.coreCount > 0 ? layout.coreCount : static_cast<int>(usage.size());
    const int fromUsage = static_cast<int>(usage.size());
    if (fromUsage > 0) return fromUsage;
    return layout.logicalCount;
}

void DrawHeatmapGrid(const cpu::CpuLayout& layout, const std::vector<float>& usage,
                     CpuUnitMode unit, const l10n::Localization& l) {
    const int count = UnitCount(layout, unit);
    if (count <= 0) return;
    const int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<float>(count))));
    const float tile = std::clamp(400.f / static_cast<float>(cols + 1), 12.f, 36.f);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    for (int i = 0; i < count; ++i) {
        const int row = i / cols;
        const int col = i % cols;
        const ImVec2 p0{start.x + col * (tile + 2.f), start.y + row * (tile + 2.f)};
        const ImVec2 p1{p0.x + tile, p0.y + tile};
        const float pct = i < static_cast<int>(usage.size()) ? usage[static_cast<size_t>(i)] : 0.f;
        dl->AddRectFilled(p0, p1, HeatColor(pct), 2.f);
        ImGui::SetCursorScreenPos(p0);
        ImGui::InvisibleButton((std::string("##hm") + std::to_string(i)).c_str(),
                               ImVec2(tile, tile));
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s %d: %.0f%%",
                        unit == CpuUnitMode::Physical
                            ? l.Get("mon", "cpu", "core_fmt").c_str()
                            : l.Get("mon", "cpu", "thread_fmt").c_str(),
                        i, pct);
            ImGui::EndTooltip();
        }
    }
    ImGui::Dummy(ImVec2(cols * (tile + 2.f), ((count + cols - 1) / cols) * (tile + 2.f)));
}

std::vector<int> SeriesInNumaGroup(const cpu::CpuLayout& layout, const std::vector<int>& group,
                                   CpuUnitMode unit) {
    if (unit == CpuUnitMode::Logical) return group;

    std::vector<int> cores;
    cores.reserve(group.size());
    for (int idx : group) {
        if (idx < 0 || idx >= static_cast<int>(layout.logicalToCore.size())) continue;
        const int core = layout.logicalToCore[static_cast<size_t>(idx)];
        if (std::find(cores.begin(), cores.end(), core) == cores.end()) cores.push_back(core);
    }
    std::sort(cores.begin(), cores.end());
    return cores;
}

void DrawNumaTimeline(const l10n::Localization& l, const cpu::CpuLayout& layout,
                      const std::vector<float>& logical, CpuUnitMode unit,
                      const std::vector<metrics::CpuSampleRow>& cpuHistory, int64_t liveTs,
                      double timeSec, int seriesCount, const char* plotId, int visibleSeconds,
                      int rangeHours, MonitorTimeAxisView& timeView) {
    const CpuTimelineData data =
        BuildTimeline(cpuHistory, layout, unit, seriesCount, logical, liveTs);
    double xMin = 0.0, xMax = 0.0;
    CpuPlotTimeBounds(data, timeSec, visibleSeconds, xMin, xMax);

    const auto groups = cpu::GroupByNuma(layout);
    for (int n = 0; n < static_cast<int>(groups.size()); ++n) {
        const auto indices = SeriesInNumaGroup(layout, groups[static_cast<size_t>(n)], unit);
        if (indices.empty()) continue;

        const std::string title = l.Get("mon", "cpu", "numa") + " " + std::to_string(n);
        ImGui::TextUnformatted(title.c_str());
        const std::string subPlotId = std::string(plotId) + "_numa" + std::to_string(n);
        if (ImPlot::BeginPlot(subPlotId.c_str(), ImVec2(-1, PlotHeight()))) {
            SetupPercentTimeAxes(l, timeView, xMin, xMax, visibleSeconds, rangeHours);
            PlotTimelineSeries(l, data, unit, &indices);
            UpdateMonitorTimeAxisX(timeView);
            ImPlot::EndPlot();
        }
    }
}

void DrawNumaHeatmap(const cpu::CpuLayout& layout, const std::vector<float>& logical,
                     CpuUnitMode unit, const l10n::Localization& l) {
    const auto groups = cpu::GroupByNuma(layout);
    for (int n = 0; n < static_cast<int>(groups.size()); ++n) {
        if (groups[n].empty()) continue;
        std::string title = l.Get("mon", "cpu", "numa") + " " + std::to_string(n);
        if (ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
            std::vector<float> slice;
            if (unit == CpuUnitMode::Physical) {
                std::map<int, float> coreMap;
                for (int idx : groups[n]) {
                    if (idx >= static_cast<int>(logical.size())) continue;
                    const int c = layout.logicalToCore[idx];
                    coreMap[c] = std::max(coreMap[c], logical[static_cast<size_t>(idx)]);
                }
                slice.resize(static_cast<size_t>(layout.coreCount), 0.f);
                for (auto& kv : coreMap) slice[static_cast<size_t>(kv.first)] = kv.second;
            } else {
                slice.resize(static_cast<size_t>(layout.logicalCount), 0.f);
                for (int idx : groups[n])
                    if (idx < static_cast<int>(logical.size()))
                        slice[static_cast<size_t>(idx)] = logical[static_cast<size_t>(idx)];
            }
            cpu::CpuLayout sub = layout;
            sub.logicalCount = static_cast<int>(groups[n].size());
            DrawHeatmapGrid(sub, slice, unit, l);
        }
    }
}

void DrawTopNTimeline(const l10n::Localization& l, const std::vector<float>& usage,
                      const cpu::CpuLayout& layout, CpuUnitMode unit, int topN,
                      const std::vector<metrics::CpuSampleRow>& cpuHistory, int64_t liveTs,
                      double timeSec, int seriesCount, const char* plotId, int visibleSeconds,
                      int rangeHours, MonitorTimeAxisView& timeView) {
    const std::vector<float> resolved = ResolveUsage(layout, usage, unit);
    const CpuTimelineData data = BuildTimeline(cpuHistory, layout, unit, seriesCount, usage, liveTs);
    double xMin = 0.0, xMax = 0.0;
    CpuPlotTimeBounds(data, timeSec, visibleSeconds, xMin, xMax);

    struct RankItem {
        int id;
        float val;
    };
    std::vector<RankItem> ranked;
    ranked.reserve(resolved.size());
    for (int i = 0; i < static_cast<int>(resolved.size()); ++i)
        ranked.push_back({i, resolved[static_cast<size_t>(i)]});
    const int top = (std::min)(topN, static_cast<int>(ranked.size()));
    std::partial_sort(ranked.begin(), ranked.begin() + top, ranked.end(),
                      [](const RankItem& a, const RankItem& b) { return a.val > b.val; });
    std::vector<int> topIds;
    topIds.reserve(static_cast<size_t>(top));
    for (int s = 0; s < top; ++s) topIds.push_back(ranked[static_cast<size_t>(s)].id);

    if (ImPlot::BeginPlot(plotId, ImVec2(-1, PlotHeight()))) {
        SetupPercentTimeAxes(l, timeView, xMin, xMax, visibleSeconds, rangeHours);
        PlotTimelineSeries(l, data, unit, &topIds);
        UpdateMonitorTimeAxisX(timeView);
        ImPlot::EndPlot();
    }
}

void DrawAllThreadsTimeline(const l10n::Localization& l, const cpu::CpuLayout& layout,
                            const std::vector<float>& usage, CpuUnitMode unit,
                            const std::vector<metrics::CpuSampleRow>& cpuHistory, int64_t liveTs,
                            double timeSec, int seriesCount, const char* plotId,
                            int visibleSeconds, int rangeHours, MonitorTimeAxisView& timeView) {
    const CpuTimelineData data = BuildTimeline(cpuHistory, layout, unit, seriesCount, usage, liveTs);
    double xMin = 0.0, xMax = 0.0;
    CpuPlotTimeBounds(data, timeSec, visibleSeconds, xMin, xMax);

    if (ImPlot::BeginPlot(plotId, ImVec2(-1, PlotHeight()))) {
        SetupPercentTimeAxes(l, timeView, xMin, xMax, visibleSeconds, rangeHours);
        PlotTimelineSeries(l, data, unit, nullptr);
        UpdateMonitorTimeAxisX(timeView);
        ImPlot::EndPlot();
    }
}

} // namespace

CpuViewMode ResolveViewMode(const CpuVizState& state, const cpu::CpuLayout& layout,
                            CpuUnitMode unit, const std::vector<float>& usage) {
    if (state.viewMode != CpuViewMode::Auto) return state.viewMode;

    const int count = ActiveUnitCount(layout, unit, usage);
    if (count < kAutoLinearMax)
        return unit == CpuUnitMode::Physical ? CpuViewMode::Cores : CpuViewMode::Threads;
    if (count <= kAutoNumaMax) return CpuViewMode::Numa;
    return CpuViewMode::Heatmap;
}

void DrawCpuVisualizer(const l10n::Localization& l, const cpu::CpuLayout& layout,
                       const std::vector<float>& usage, CpuVizState& state, int64_t liveSampleKey,
                       double timeUnixSec, const char* plotId, int visibleSeconds, int rangeHours,
                       MonitorTimeAxisView& timeView,
                       const std::vector<metrics::CpuSampleRow>& cpuHistory) {
    const char* modes[] = {"auto", "threads", "cores", "numa", "heatmap", "topn"};
    CpuViewMode modesEnum[] = {CpuViewMode::Auto, CpuViewMode::Threads, CpuViewMode::Cores,
                               CpuViewMode::Numa, CpuViewMode::Heatmap, CpuViewMode::TopN};

    ImGui::PushID("cpu_view_modes");
    ImGui::BeginGroup();
    for (int i = 0; i < 6; ++i) {
        if (i > 0) ImGui::SameLine();
        const bool sel = state.viewMode == modesEnum[i];
        const std::string label = l.Get("mon", "cpu", modes[i]) + "##" + modes[i];
        if (ImGui::RadioButton(label.c_str(), sel)) {
            state.viewMode = modesEnum[i];
            state.manualOverride = state.viewMode != CpuViewMode::Auto;
        }
    }
    ImGui::EndGroup();
    ImGui::PopID();

    ImGui::SameLine(0.f, 12.f);
    ImGui::TextUnformatted("|");

    ImGui::PushID("cpu_unit_modes");
    {
        const std::string lbl = l.Get("mon", "cpu", "logical") + "##logical";
        if (ImGui::RadioButton(lbl.c_str(), state.unitMode == CpuUnitMode::Logical))
            state.unitMode = CpuUnitMode::Logical;
    }
    ImGui::SameLine();
    {
        const std::string lbl = l.Get("mon", "cpu", "physical") + "##physical";
        if (ImGui::RadioButton(lbl.c_str(), state.unitMode == CpuUnitMode::Physical))
            state.unitMode = CpuUnitMode::Physical;
    }
    ImGui::PopID();

    if (state.viewMode == CpuViewMode::TopN) {
        ImGui::SetNextItemWidth(80.f);
        ImGui::SliderInt((l.Get("mon", "cpu", "topn_count") + "##topn").c_str(), &state.topN, 3, 10);
    }

    if (state.viewMode == CpuViewMode::Auto) state.manualOverride = false;

    const std::vector<float> resolved = ResolveUsage(layout, usage, state.unitMode);
    const CpuViewMode effective = ResolveViewMode(state, layout, state.unitMode, usage);
    const int seriesCount = ActiveUnitCount(layout, state.unitMode, usage);
    const double plotTime = NormalizeTimeSec(timeUnixSec);

    if (layout.logicalCount > 64 && state.viewMode == CpuViewMode::Threads && state.manualOverride) {
        ImGui::TextColored(ImVec4(1.f, 0.7f, 0.2f, 1.f), "%s", l.Get("mon", "cpu", "many_warn").c_str());
    }

    const int64_t liveTs = timeUnixSec > 0 ? static_cast<int64_t>(timeUnixSec) : 0;
    (void)liveSampleKey;

    switch (effective) {
    case CpuViewMode::Heatmap:
        DrawHeatmapGrid(layout, resolved, state.unitMode, l);
        break;
    case CpuViewMode::Numa:
        if (state.viewMode == CpuViewMode::Auto)
            DrawNumaTimeline(l, layout, usage, state.unitMode, cpuHistory, liveTs, plotTime,
                             seriesCount, plotId, visibleSeconds, rangeHours, timeView);
        else
            DrawNumaHeatmap(layout, usage, state.unitMode, l);
        break;
    case CpuViewMode::TopN:
        DrawTopNTimeline(l, usage, layout, state.unitMode, state.topN, cpuHistory, liveTs, plotTime,
                         seriesCount, plotId, visibleSeconds, rangeHours, timeView);
        break;
    case CpuViewMode::Threads:
    case CpuViewMode::Cores:
        if (ActiveUnitCount(layout, state.unitMode, usage) <= 64)
            DrawAllThreadsTimeline(l, layout, usage, state.unitMode, cpuHistory, liveTs, plotTime,
                                   seriesCount, plotId, visibleSeconds, rangeHours, timeView);
        else
            DrawHeatmapGrid(layout, resolved, state.unitMode, l);
        break;
    case CpuViewMode::Auto:
    default:
        DrawAllThreadsTimeline(l, layout, usage, state.unitMode, cpuHistory, liveTs, plotTime,
                               seriesCount, plotId, visibleSeconds, rangeHours, timeView);
        break;
    }
}

} // namespace maku::ui
