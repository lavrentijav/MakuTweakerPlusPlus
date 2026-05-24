#include "ui/MonitorPage.h"
#include "ui/CpuVisualizer.h"
#include "ui/MonitorPlotAxis.h"
#include "app/Application.h"
#include "core/MetricsBackground.h"
#include "core/MetricsDb.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include "platform/MetricsService.h"
#include "platform/MetricsTray.h"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <ctime>
#include <functional>
#include <string>

namespace maku::ui::pages {
namespace {

using app::Application;

enum class ResourceId { Cpu, Gpu, Ram, Net, Disk };

struct MonitorUiState {
    ResourceId selected = ResourceId::Cpu;
    int rangeHours = 1;
    int visibleMinutes = 10;
    CpuVizState cpuViz{};
    std::vector<metrics::SystemSample> cachedSamples;
    std::vector<metrics::CpuSampleRow> cachedCpuRows;
    int cachedRangeHours = -1;
    int cachedCpuRangeHours = -1;
    double lastDbFetch = 0.0;
    double lastCpuDbFetch = 0.0;
    int64_t cachedSampleKey = -1;
    int64_t cachedCpuSampleKey = -1;
    int64_t lastLiveMergedKey = -1;
    std::string serviceMsg;
    MonitorTimeAxisView timeView{};
    ResourceId timeViewResource = ResourceId::Cpu;
};

MonitorUiState& State() {
    static MonitorUiState s;
    return s;
}

int64_t RangeSinceTs(int hours) {
    return static_cast<int64_t>(time(nullptr)) - static_cast<int64_t>(hours) * 3600;
}

int MaxVisibleMinutes(int rangeHours) {
    return std::max(1, rangeHours * 60);
}

int VisibleSeconds(const MonitorUiState& st) {
    return std::max(60, std::min(st.visibleMinutes, MaxVisibleMinutes(st.rangeHours)) * 60);
}

void InvalidateMonitorCaches(MonitorUiState& st) {
    st.cachedSamples.clear();
    st.cachedCpuRows.clear();
    st.cachedRangeHours = -1;
    st.cachedCpuRangeHours = -1;
    st.cachedSampleKey = -1;
    st.cachedCpuSampleKey = -1;
    st.lastLiveMergedKey = -1;
    st.lastDbFetch = 0.0;
    st.lastCpuDbFetch = 0.0;
}

void OnMetricsServiceRunning(app::Application& app) {
    if (!metrics_svc::IsRunningCached()) return;
    if (metrics_tray::IsForegroundGuiProcess()) {
        metrics_tray::EnsureStandaloneTray();
        return;
    }
    app.AdoptMetricsServiceTrayMode();
}

void MergeLiveIntoCaches(MonitorUiState& st, const metrics::LiveSample& live, int64_t sampleKey,
                         int rangeHours) {
    if (sampleKey == st.lastLiveMergedKey || live.system.ts <= 0) return;
    st.lastLiveMergedKey = sampleKey;

    const int64_t since = RangeSinceTs(rangeHours);

    st.cachedSamples.erase(
        std::remove_if(st.cachedSamples.begin(), st.cachedSamples.end(),
                       [&](const metrics::SystemSample& s) { return s.ts == live.system.ts; }),
        st.cachedSamples.end());
    st.cachedSamples.push_back(live.system);
    std::sort(st.cachedSamples.begin(), st.cachedSamples.end(),
              [](const metrics::SystemSample& a, const metrics::SystemSample& b) {
                  return a.ts < b.ts;
              });
    st.cachedSamples.erase(std::remove_if(st.cachedSamples.begin(), st.cachedSamples.end(),
                                            [since](const metrics::SystemSample& s) {
                                                return s.ts < since;
                                            }),
                           st.cachedSamples.end());

    if (live.perLogical.empty()) return;

    st.cachedCpuRows.erase(
        std::remove_if(st.cachedCpuRows.begin(), st.cachedCpuRows.end(),
                       [&](const metrics::CpuSampleRow& r) { return r.ts == live.system.ts; }),
        st.cachedCpuRows.end());
    for (int i = 0; i < static_cast<int>(live.perLogical.size()); ++i) {
        st.cachedCpuRows.push_back(
            metrics::CpuSampleRow{live.system.ts, i, live.perLogical[static_cast<size_t>(i)]});
    }
    std::sort(st.cachedCpuRows.begin(), st.cachedCpuRows.end(),
              [](const metrics::CpuSampleRow& a, const metrics::CpuSampleRow& b) {
                  if (a.ts != b.ts) return a.ts < b.ts;
                  return a.procIndex < b.procIndex;
              });
    st.cachedCpuRows.erase(std::remove_if(st.cachedCpuRows.begin(), st.cachedCpuRows.end(),
                                            [since](const metrics::CpuSampleRow& r) {
                                                return r.ts < since;
                                            }),
                           st.cachedCpuRows.end());
}

void RefreshDbCache(MonitorUiState& st, metrics::MetricsDb& db, int64_t liveSampleKey,
                    bool svcRunning) {
    const double now = ImGui::GetTime();
    const double minInterval = svcRunning ? 2.0 : 2.0;
    const bool canSkip = !st.cachedSamples.empty() && st.cachedRangeHours == st.rangeHours &&
                         (!svcRunning && liveSampleKey == st.cachedSampleKey) &&
                         now - st.lastDbFetch < minInterval;
    if (canSkip) return;
    st.lastDbFetch = now;
    st.cachedRangeHours = st.rangeHours;
    st.cachedSampleKey = liveSampleKey;
    st.cachedSamples = db.QuerySystem(RangeSinceTs(st.rangeHours), 0, 4096);
}

void RefreshCpuCache(MonitorUiState& st, metrics::MetricsDb& db, int rangeHours,
                     int64_t liveSampleKey, bool svcRunning) {
    const double now = ImGui::GetTime();
    const double minInterval = svcRunning ? 2.0 : 2.0;
    const bool canSkip = !st.cachedCpuRows.empty() && st.cachedCpuRangeHours == rangeHours &&
                         (!svcRunning && liveSampleKey == st.cachedCpuSampleKey) &&
                         now - st.lastCpuDbFetch < minInterval;
    if (canSkip) return;
    st.lastCpuDbFetch = now;
    st.cachedCpuRangeHours = rangeHours;
    st.cachedCpuSampleKey = liveSampleKey;
    st.cachedCpuRows = db.QueryCpu(RangeSinceTs(rangeHours));
}

bool InstallServiceUi(Settings& settings, MonitorUiState& st, const l10n::Localization& l,
                      app::Application& app) {
    if (metrics_svc::InstallElevated()) {
        settings.metricsServiceEnabled = true;
        settings.Save();
        st.serviceMsg = l.Get("mon", "main", "service_install_ok");
        InvalidateMonitorCaches(st);
        OnMetricsServiceRunning(app);
        return true;
    }
    st.serviceMsg = l.Get("mon", "main", "service_install_fail");
    return false;
}

void DrawSparklineAt(ImDrawList* dl, ImVec2 origin, ImVec2 size, const std::vector<float>& values,
                     ImU32 color) {
    if (values.size() < 2) return;
    const float w = size.x;
    const float h = size.y;
    const float step = w / static_cast<float>(values.size() - 1);
    for (size_t i = 1; i < values.size(); ++i) {
        const float y0 = origin.y + h - (std::clamp(values[i - 1], 0.f, 100.f) / 100.f) * h;
        const float y1 = origin.y + h - (std::clamp(values[i], 0.f, 100.f) / 100.f) * h;
        dl->AddLine(ImVec2(origin.x + step * static_cast<float>(i - 1), y0),
                    ImVec2(origin.x + step * static_cast<float>(i), y1), color, 1.5f);
    }
}

bool DrawResourceTile(const char* id, const std::string& title, const std::string& subtitle,
                      const std::vector<float>& spark, bool selected) {
    constexpr float kTileH = 72.f;
    const float tileW = ImGui::GetContentRegionAvail().x;
    if (selected) ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
    const bool clicked =
        ImGui::Selectable(id, selected, ImGuiSelectableFlags_None, ImVec2(tileW, kTileH));
    if (selected) ImGui::PopStyleColor();

    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(mn.x + 10.f, mn.y + 10.f), IM_COL32(235, 235, 245, 255), title.c_str());
    dl->AddText(ImVec2(mn.x + 10.f, mn.y + 30.f), IM_COL32(150, 155, 175, 255), subtitle.c_str());
    const float sparkW = std::max(40.f, mx.x - mn.x - 20.f);
    DrawSparklineAt(dl, ImVec2(mn.x + 10.f, mn.y + 48.f), ImVec2(sparkW, 18.f), spark,
                    IM_COL32(80, 160, 255, 255));
    return clicked;
}

std::vector<float> SparkFromSystem(const std::vector<metrics::SystemSample>& samples,
                                   float metrics::SystemSample::* field) {
    std::vector<float> out;
    out.reserve(samples.size());
    for (const auto& s : samples) out.push_back(s.*field);
    if (out.size() > 60) out.erase(out.begin(), out.end() - 60);
    return out;
}

std::vector<float> SparkFromNet(const std::vector<metrics::SystemSample>& samples) {
    std::vector<float> out;
    out.reserve(samples.size());
    for (const auto& s : samples) {
        const float kb = static_cast<float>(s.netDown) / 1024.f;
        out.push_back(std::min(kb, 100.f));
    }
    if (out.size() > 60) out.erase(out.begin(), out.end() - 60);
    return out;
}

void DrawSystemGraphValues(const l10n::Localization& l, const std::vector<metrics::SystemSample>& samples,
                           const char* plotId, const char* yLabel, const char* seriesLabel,
                           bool percentAxis, int visibleSeconds, int rangeHours,
                           MonitorTimeAxisView& timeView,
                           const std::function<double(const metrics::SystemSample&)>& valueFn) {
    std::vector<double> xs;
    std::vector<double> ys;
    if (samples.empty()) {
        xs.push_back(static_cast<double>(time(nullptr)));
        ys.push_back(0.0);
    } else {
        xs.resize(samples.size());
        ys.resize(samples.size());
        for (size_t i = 0; i < samples.size(); ++i) {
            const double ts = static_cast<double>(samples[i].ts);
            xs[i] = ts > 0.0 ? ts : static_cast<double>(time(nullptr));
            ys[i] = valueFn(samples[i]);
        }
    }

    const float plotH = std::max(220.f, ImGui::GetContentRegionAvail().y - 4.f);
    if (ImPlot::BeginPlot(plotId, ImVec2(-1, plotH))) {
        ImPlot::SetupAxes(l.Get("mon", "graph", "time").c_str(), yLabel);
        ImPlot::SetupAxisScale(ImAxis_X1, ImPlotScale_Time);
        if (percentAxis) {
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0, 100.0, ImPlotCond_Always);
            ImPlot::SetupAxisLimitsConstraints(ImAxis_Y1, 0.0, 100.0);
        }
        if (!xs.empty()) {
            const double xMax = xs.back();
            const double xMin = xs.front();
            SetupMonitorTimeAxisX(timeView, xMin, xMax, visibleSeconds, rangeHours);
        }
        ImPlot::SetupLegend(ImPlotLocation_NorthEast, ImPlotLegendFlags_Outside);

        static thread_local std::vector<double> filteredX, filteredY, plotX, plotY;
        const int budget = PlotDownsampleBudget();
        const ImPlotRect lim = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
        FilterSeriesByTime(xs, ys, lim.X.Min, lim.X.Max, filteredX, filteredY);
        DownsampleAverage(filteredX, filteredY, plotX, plotY, budget);
        if (!plotX.empty())
            ImPlot::PlotLine(seriesLabel, plotX.data(), plotY.data(), static_cast<int>(plotX.size()));

        UpdateMonitorTimeAxisX(timeView);
        ImPlot::EndPlot();
    }
}

void DrawSystemGraph(const l10n::Localization& l, const std::vector<metrics::SystemSample>& samples,
                     float metrics::SystemSample::* field, const char* plotId, const char* yLabel,
                     const char* seriesLabel, bool percentAxis, int visibleSeconds, int rangeHours,
                     MonitorTimeAxisView& timeView) {
    DrawSystemGraphValues(
        l, samples, plotId, yLabel, seriesLabel, percentAxis, visibleSeconds, rangeHours, timeView,
        [field, percentAxis](const metrics::SystemSample& s) {
            const double v = static_cast<double>(s.*field);
            return percentAxis ? static_cast<double>(std::clamp(static_cast<float>(v), 0.f, 100.f)) : v;
        });
}

} // namespace

void DrawMonitor() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    auto& settings = app.GetSettings();
    auto& st = State();
    auto& bg = metrics::MetricsBackground::Instance();
    auto& db = bg.Db();

    const auto& live = bg.Live();
    const auto& liveSystem = live.system;
    const auto& liveLogical = live.perLogical;
    const int64_t liveSampleKey = bg.SampleKey();
    auto& layout = bg.Layout();

    const int maxVisibleMin = MaxVisibleMinutes(st.rangeHours);
    st.visibleMinutes = std::clamp(st.visibleMinutes, 1, maxVisibleMin);
    const int visibleSec = VisibleSeconds(st);

    const bool svcRunning = metrics_svc::IsRunningCached();
    RefreshDbCache(st, db, liveSampleKey, svcRunning);
    RefreshCpuCache(st, db, st.rangeHours, liveSampleKey, svcRunning);
    if (svcRunning) MergeLiveIntoCaches(st, live, liveSampleKey, st.rangeHours);
    const auto& sysSamples = st.cachedSamples;

    if (ImGui::BeginTable("mon_toolbar", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadInnerX)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", util::ToUtf8(db.DefaultPath()).c_str());
        ImGui::TableSetColumnIndex(1);
        wchar_t svcStatus[64]{};
        metrics_svc::GetStatus(svcStatus, 64);
        ImGui::Text("%s: %ls", l.Get("mon", "main", "service_status").c_str(), svcStatus);
        if (!metrics_svc::IsInstalledCached()) {
            ImGui::SameLine();
            if (ImGui::Button(l.Get("mon", "main", "service_install").c_str()))
                InstallServiceUi(settings, st, l, app);
        } else if (!metrics_svc::IsRunningCached()) {
            ImGui::SameLine();
            if (ImGui::Button(l.Get("mon", "main", "service_start").c_str())) {
                if (metrics_svc::Start() || metrics_svc::InstallElevated()) {
                    st.serviceMsg = l.Get("mon", "main", "service_install_ok");
                    InvalidateMonitorCaches(st);
                    OnMetricsServiceRunning(app);
                } else
                    st.serviceMsg = l.Get("mon", "main", "service_install_fail");
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", l.Get("mon", "main", "gui_collect_hint").c_str());
        }

        if (!st.serviceMsg.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.f), "%s", st.serviceMsg.c_str());
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        bool svcEnabled = settings.metricsServiceEnabled;
        if (ImGui::Checkbox(l.Get("mon", "main", "service_enable").c_str(), &svcEnabled)) {
            settings.metricsServiceEnabled = svcEnabled;
            settings.Save();
            if (svcEnabled) {
                if (!metrics_svc::IsInstalled())
                    InstallServiceUi(settings, st, l, app);
                else {
                    metrics_svc::EnsureAutostartAndRunning();
                    InvalidateMonitorCaches(st);
                    OnMetricsServiceRunning(app);
                }
            } else {
                metrics_svc::Stop();
                InvalidateMonitorCaches(st);
            }
        }
        ImGui::TableSetColumnIndex(1);
        const char* rangeKeys[] = {"1h", "6h", "24h", "7d", "30d"};
        const int rangeHours[] = {1, 6, 24, 168, 720};
        for (int i = 0; i < 5; ++i) {
            if (i > 0) ImGui::SameLine();
            if (ImGui::RadioButton(l.Get("mon", "main", rangeKeys[i]).c_str(),
                                   st.rangeHours == rangeHours[i])) {
                st.rangeHours = rangeHours[i];
                st.visibleMinutes = std::min(st.visibleMinutes, MaxVisibleMinutes(st.rangeHours));
            }
        }
        ImGui::EndTable();
    }

    ImGui::SetNextItemWidth(220.f);
    ImGui::SliderInt(l.Get("mon", "main", "zoom").c_str(), &st.visibleMinutes, 1, maxVisibleMin,
                     "%d min");

    ImGui::Spacing();
    const float sidebarW = 220.f;
    ImGui::BeginChild("mon_sidebar", ImVec2(sidebarW, 0), ImGuiChildFlags_Borders);
    ImGui::PushItemWidth(-1.f);
    {
        auto cpuSpark = SparkFromSystem(sysSamples, &metrics::SystemSample::cpuTotal);
        if (cpuSpark.empty() && liveSystem.cpuTotal > 0) cpuSpark.push_back(liveSystem.cpuTotal);
        const std::string cpuSub = std::to_string(static_cast<int>(liveSystem.cpuTotal)) + "%";
        if (DrawResourceTile("##cpu", l.Get("mon", "res", "cpu"), cpuSub, cpuSpark,
                             st.selected == ResourceId::Cpu))
            st.selected = ResourceId::Cpu;

        auto gpuSpark = SparkFromSystem(sysSamples, &metrics::SystemSample::gpuPct);
        if (gpuSpark.empty() && liveSystem.gpuPct > 0) gpuSpark.push_back(liveSystem.gpuPct);
        const std::string gpuSub = std::to_string(static_cast<int>(liveSystem.gpuPct)) + "%";
        if (DrawResourceTile("##gpu", l.Get("mon", "res", "gpu"), gpuSub, gpuSpark,
                             st.selected == ResourceId::Gpu))
            st.selected = ResourceId::Gpu;

        auto ramSpark = SparkFromSystem(sysSamples, &metrics::SystemSample::ramPct);
        if (ramSpark.empty() && liveSystem.ramPct > 0) ramSpark.push_back(liveSystem.ramPct);
        const std::string ramSub = std::to_string(static_cast<int>(liveSystem.ramPct)) + "%";
        if (DrawResourceTile("##ram", l.Get("mon", "res", "ram"), ramSub, ramSpark,
                             st.selected == ResourceId::Ram))
            st.selected = ResourceId::Ram;

        auto netSpark = SparkFromNet(sysSamples);
        const std::string netSub = std::to_string(liveSystem.netDown / 1024) + " KB/s";
        if (DrawResourceTile("##net", l.Get("mon", "res", "net"), netSub, netSpark,
                             st.selected == ResourceId::Net))
            st.selected = ResourceId::Net;

        auto diskSpark = SparkFromSystem(sysSamples, &metrics::SystemSample::diskPct);
        const std::string diskSub = std::to_string(static_cast<int>(liveSystem.diskPct)) + "%";
        if (DrawResourceTile("##disk", l.Get("mon", "res", "disk"), diskSub, diskSpark,
                             st.selected == ResourceId::Disk))
            st.selected = ResourceId::Disk;
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("mon_detail", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (st.timeViewResource != st.selected) {
        st.timeView.manualX = false;
        st.timeViewResource = st.selected;
    }
    switch (st.selected) {
    case ResourceId::Cpu:
        ImGui::TextDisabled("%s  ·  %d%%", l.Get("mon", "res", "cpu").c_str(),
                            static_cast<int>(liveSystem.cpuTotal));
        ImGui::Separator();
        DrawCpuVisualizer(l, layout, liveLogical, st.cpuViz, liveSampleKey,
                          liveSystem.ts > 0 ? static_cast<double>(liveSystem.ts)
                                            : static_cast<double>(time(nullptr)),
                          "cpu_plot", visibleSec, st.rangeHours, st.timeView, st.cachedCpuRows);
        break;
    case ResourceId::Gpu:
        DrawSystemGraph(l, sysSamples, &metrics::SystemSample::gpuPct, "gpu_plot",
                        l.Get("mon", "graph", "pct").c_str(), l.Get("mon", "res", "gpu").c_str(),
                        true, visibleSec, st.rangeHours, st.timeView);
        break;
    case ResourceId::Ram:
        DrawSystemGraph(l, sysSamples, &metrics::SystemSample::ramPct, "ram_plot",
                        l.Get("mon", "graph", "pct").c_str(), l.Get("mon", "res", "ram").c_str(),
                        true, visibleSec, st.rangeHours, st.timeView);
        break;
    case ResourceId::Net:
        ImGui::Text("%s: %lld B/s", l.Get("mon", "res", "net").c_str(),
                    static_cast<long long>(liveSystem.netDown));
        DrawSystemGraphValues(l, sysSamples, "net_plot", l.Get("mon", "graph", "speed").c_str(),
                              l.Get("mon", "res", "net").c_str(), false, visibleSec, st.rangeHours,
                              st.timeView, [](const metrics::SystemSample& s) {
                                  return static_cast<double>(s.netDown) / 1024.0;
                              });
        break;
    case ResourceId::Disk:
        DrawSystemGraph(l, sysSamples, &metrics::SystemSample::diskPct, "disk_plot",
                        l.Get("mon", "graph", "pct").c_str(), l.Get("mon", "res", "disk").c_str(),
                        true, visibleSec, st.rangeHours, st.timeView);
        break;
    }
    ImGui::EndChild();
}

} // namespace maku::ui::pages
