#include "ui/MonitorPage.h"
#include "ui/CpuVisualizer.h"
#include "ui/MonitorPlotAxis.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"
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

struct ChartWindow {
    int64_t sinceTs = 0;
    int64_t untilTs = 0;
};

struct MonitorUiState {
    ResourceId selected = ResourceId::Cpu;
    int rangeHours = 1;
    int visibleMinutes = 10;
    CpuVizState cpuViz{};
    std::vector<metrics::SystemSample> cachedChartSamples;
    std::vector<metrics::SystemSample> cachedSparkSamples;
    std::vector<metrics::CpuSampleRow> cachedCpuRows;
    ChartWindow cachedChartWindow{};
    ChartWindow cachedCpuWindow{};
    ResourceId cachedChartResource = ResourceId::Cpu;
    int cachedRangeHours = -1;
    double lastChartDbFetch = 0.0;
    double lastCpuDbFetch = 0.0;
    double lastSparkDbFetch = 0.0;
    int64_t cachedSampleKey = -1;
    int64_t cachedCpuSampleKey = -1;
    int64_t lastLiveMergedKey = -1;
    std::string serviceMsg;
    MonitorTimeAxisView timeView{};
    ResourceId timeViewResource = ResourceId::Cpu;
    /// Width of one averaged chart point, shown next to the zoom slider.
    double lastBucketSeconds = 0.0;
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
    st.cachedChartSamples.clear();
    st.cachedSparkSamples.clear();
    st.cachedCpuRows.clear();
    st.cachedChartWindow = {};
    st.cachedCpuWindow = {};
    st.cachedRangeHours = -1;
    st.cachedSampleKey = -1;
    st.cachedCpuSampleKey = -1;
    st.lastLiveMergedKey = -1;
    st.lastChartDbFetch = 0.0;
    st.lastCpuDbFetch = 0.0;
    st.lastSparkDbFetch = 0.0;
}

bool ChartWindowNear(const ChartWindow& a, const ChartWindow& b) {
    return std::abs(a.sinceTs - b.sinceTs) <= 45 && std::abs(a.untilTs - b.untilTs) <= 45;
}

ChartWindow ComputeChartWindow(const MonitorUiState& st) {
    const int64_t now = static_cast<int64_t>(time(nullptr));
    const int64_t rangeFloor = RangeSinceTs(st.rangeHours);
    const int visibleSec = VisibleSeconds(st);
    const int marginSec = std::max(90, visibleSec / 3);

    ChartWindow w;
    w.untilTs = now + 2;
    w.sinceTs = now - static_cast<int64_t>(visibleSec) - marginSec;

    if (st.timeView.manualX && st.timeView.savedXMax > st.timeView.savedXMin) {
        w.sinceTs = static_cast<int64_t>(st.timeView.savedXMin) - marginSec;
        w.untilTs = static_cast<int64_t>(st.timeView.savedXMax) + marginSec;
    }

    w.sinceTs = std::max(w.sinceTs, rangeFloor);
    w.untilTs = std::min(w.untilTs, now + 2);
    if (w.untilTs <= w.sinceTs) w.untilTs = w.sinceTs + visibleSec;
    return w;
}

int SystemMaxRowsForWindow(const ChartWindow& w) {
    const int64_t span = std::max<int64_t>(60, w.untilTs - w.sinceTs);
    return std::clamp(static_cast<int>(span / 2) + 32, 96, 2048);
}

int CpuMaxRowsForWindow(const ChartWindow& w, int logicalCount) {
    const int cores = std::max(1, logicalCount);
    const int64_t span = std::max<int64_t>(60, w.untilTs - w.sinceTs);
    const int est = static_cast<int>(span / 2) + 16;
    return std::clamp(est * cores, 2048, 48000);
}

void OnMetricsServiceRunning(app::Application& app) {
    if (!metrics_svc::IsRunningCached()) return;
    if (metrics_tray::IsForegroundGuiProcess()) {
        metrics_tray::EnsureStandaloneTray();
        return;
    }
    app.AdoptMetricsServiceTrayMode();
}

void MergeLiveIntoChart(MonitorUiState& st, const metrics::LiveSample& live, int64_t sampleKey,
                        const ChartWindow& window) {
    if (sampleKey == st.lastLiveMergedKey || live.system.ts <= 0) return;
    st.lastLiveMergedKey = sampleKey;
    if (live.system.ts < window.sinceTs || live.system.ts > window.untilTs) return;

    auto mergeSystem = [&](std::vector<metrics::SystemSample>& samples) {
        samples.erase(std::remove_if(samples.begin(), samples.end(),
                                     [&](const metrics::SystemSample& s) {
                                         return s.ts == live.system.ts;
                                     }),
                        samples.end());
        samples.push_back(live.system);
        std::sort(samples.begin(), samples.end(),
                  [](const metrics::SystemSample& a, const metrics::SystemSample& b) {
                      return a.ts < b.ts;
                  });
        samples.erase(std::remove_if(samples.begin(), samples.end(),
                                     [&](const metrics::SystemSample& s) {
                                         return s.ts < window.sinceTs || s.ts > window.untilTs;
                                     }),
                        samples.end());
    };
    mergeSystem(st.cachedChartSamples);
    mergeSystem(st.cachedSparkSamples);
}

void RefreshSparkCache(MonitorUiState& st, metrics::MetricsDb& db, double refreshSec) {
    const double now = ImGui::GetTime();
    const double minInterval = std::max(2.0, refreshSec * 2.0);
    if (!st.cachedSparkSamples.empty() && now - st.lastSparkDbFetch < minInterval) return;

    const int64_t end = static_cast<int64_t>(time(nullptr)) + 1;
    const int64_t since = end - 1200;
    st.lastSparkDbFetch = now;
    st.cachedSparkSamples = db.QuerySystem(since, end, 96);
}

void RefreshChartSystemCache(MonitorUiState& st, metrics::MetricsDb& db, ResourceId resource,
                             const ChartWindow& window, int64_t liveSampleKey, bool svcRunning,
                             double refreshSec) {
    const double now = ImGui::GetTime();
    const double minInterval = std::max(1.0, refreshSec);
    const bool canSkip =
        !st.cachedChartSamples.empty() && st.cachedChartResource == resource &&
        st.cachedRangeHours == st.rangeHours && ChartWindowNear(st.cachedChartWindow, window) &&
        (!svcRunning && liveSampleKey == st.cachedSampleKey) && now - st.lastChartDbFetch < minInterval;
    if (canSkip) return;

    st.lastChartDbFetch = now;
    st.cachedChartResource = resource;
    st.cachedRangeHours = st.rangeHours;
    st.cachedChartWindow = window;
    st.cachedSampleKey = liveSampleKey;
    st.cachedChartSamples =
        db.QuerySystem(window.sinceTs, window.untilTs, SystemMaxRowsForWindow(window));
}

void RefreshCpuCache(MonitorUiState& st, metrics::MetricsDb& db, const ChartWindow& window,
                     int logicalCount, int64_t liveSampleKey, bool svcRunning, double refreshSec) {
    const double now = ImGui::GetTime();
    const double minInterval = std::max(1.0, refreshSec);
    const bool canSkip = !st.cachedCpuRows.empty() && ChartWindowNear(st.cachedCpuWindow, window) &&
                         (!svcRunning && liveSampleKey == st.cachedCpuSampleKey) &&
                         now - st.lastCpuDbFetch < minInterval;
    if (canSkip) return;

    st.lastCpuDbFetch = now;
    st.cachedCpuWindow = window;
    st.cachedCpuSampleKey = liveSampleKey;
    st.cachedCpuRows =
        db.QueryCpu(window.sinceTs, -1, window.untilTs, CpuMaxRowsForWindow(window, logicalCount));
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
                      const std::vector<float>& spark, bool selected, bool dark) {
    constexpr float kTileH = 58.f;
    const float tileW = ImGui::GetContentRegionAvail().x;
    const ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 cardCol = ImGui::ColorConvertFloat4ToU32(CardSurfaceColor(dark));
    const ImVec2 tileMax(cursor.x + tileW, cursor.y + kTileH);
    dl->AddRectFilled(cursor, tileMax, cardCol, TileRounding());
    if (selected)
        dl->AddRectFilled(cursor, ImVec2(cursor.x + 3.f, cursor.y + kTileH), AccentBarU32(), 4.f);
    ImGui::InvisibleButton(id, ImVec2(tileW, kTileH));
    const bool clicked = ImGui::IsItemClicked();

    const ImVec2 mn = ImGui::GetItemRectMin();
    const ImVec2 mx = ImGui::GetItemRectMax();
    dl->AddText(ImVec2(mn.x + 10.f, mn.y + 6.f), IM_COL32(235, 230, 220, 255), title.c_str());
    dl->AddText(ImVec2(mn.x + 10.f, mn.y + 20.f), IM_COL32(160, 150, 130, 255), subtitle.c_str());
    const float sparkW = std::max(40.f, mx.x - mn.x - 20.f);
    DrawSparklineAt(dl, ImVec2(mn.x + 10.f, mn.y + 38.f), ImVec2(sparkW, 14.f), spark,
                    AccentSparklineU32());
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
                           const std::function<double(const metrics::SystemSample&)>& valueFn,
                           double* outBucketSeconds = nullptr) {
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
        // Average into wall-clock buckets: an hour-wide view becomes one point
        // per minute instead of a dense unreadable line.
        const double bucket = NiceBucketSeconds(lim.X.Max - lim.X.Min, budget);
        DownsampleTimeBuckets(filteredX, filteredY, plotX, plotY, bucket);
        if (!plotX.empty())
            ImPlot::PlotLine(seriesLabel, plotX.data(), plotY.data(), static_cast<int>(plotX.size()));
        if (outBucketSeconds) *outBucketSeconds = bucket;

        UpdateMonitorTimeAxisX(timeView);
        ImPlot::EndPlot();
    }
}

void DrawSystemGraph(const l10n::Localization& l, const std::vector<metrics::SystemSample>& samples,
                     float metrics::SystemSample::* field, const char* plotId, const char* yLabel,
                     const char* seriesLabel, bool percentAxis, int visibleSeconds, int rangeHours,
                     MonitorTimeAxisView& timeView, double* outBucketSeconds = nullptr) {
    DrawSystemGraphValues(
        l, samples, plotId, yLabel, seriesLabel, percentAxis, visibleSeconds, rangeHours, timeView,
        [field, percentAxis](const metrics::SystemSample& s) {
            const double v = static_cast<double>(s.*field);
            return percentAxis ? static_cast<double>(std::clamp(static_cast<float>(v), 0.f, 100.f)) : v;
        },
        outBucketSeconds);
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
    const double refreshSec =
        std::max(1.0, static_cast<double>(std::max(1000, settings.monitoringRefreshMs)) / 1000.0);
    const ChartWindow chartWindow = ComputeChartWindow(st);

    RefreshSparkCache(st, db, refreshSec);

    if (st.selected != ResourceId::Cpu) {
        if (!st.cachedCpuRows.empty()) {
            st.cachedCpuRows.clear();
            st.cachedCpuWindow = {};
        }
        RefreshChartSystemCache(st, db, st.selected, chartWindow, liveSampleKey, svcRunning,
                                refreshSec);
    } else {
        RefreshCpuCache(st, db, chartWindow, layout.logicalCount, liveSampleKey, svcRunning,
                        refreshSec);
    }

    if (svcRunning) MergeLiveIntoChart(st, live, liveSampleKey, chartWindow);

    const auto& sysSamples = st.cachedChartSamples;
    const auto& sparkSamples = st.cachedSparkSamples;

    const bool dark = IsDarkTheme(settings.theme);

    PushCompactToolbarStyle();

    // Two plain rows instead of a table with a hard-coded 360px right column:
    // that column clipped the range buttons on narrower windows. Everything
    // here sizes to its own content.
    {
        bool svcEnabled = settings.metricsServiceEnabled;
        if (ImGui::Checkbox(l.Get("mon", "main", "service_enable_short").c_str(), &svcEnabled)) {
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
        ImGui::SameLine(0.f, 8.f);
        wchar_t svcStatus[64]{};
        metrics_svc::GetStatus(svcStatus, 64);
        ImGui::AlignTextToFramePadding();
        ImGui::Text("%s: %ls", l.Get("mon", "main", "service_status").c_str(), svcStatus);
        if (!metrics_svc::IsInstalledCached()) {
            ImGui::SameLine(0.f, 6.f);
            if (ImGui::Button(l.Get("mon", "main", "service_install").c_str()))
                InstallServiceUi(settings, st, l, app);
        } else if (!metrics_svc::IsRunningCached()) {
            ImGui::SameLine(0.f, 6.f);
            if (ImGui::Button(l.Get("mon", "main", "service_start").c_str())) {
                if (metrics_svc::Start() || metrics_svc::InstallElevated()) {
                    st.serviceMsg = l.Get("mon", "main", "service_install_ok");
                    InvalidateMonitorCaches(st);
                    OnMetricsServiceRunning(app);
                } else
                    st.serviceMsg = l.Get("mon", "main", "service_install_fail");
            }
        }
        if (!st.serviceMsg.empty()) {
            ImGui::SameLine(0.f, 6.f);
            ImGui::TextColored(ImVec4(0.9f, 0.75f, 0.3f, 1.f), "%s", st.serviceMsg.c_str());
        }
    }

    {
        const char* rangeKeys[] = {"1h", "6h", "24h", "7d", "30d"};
        const int rangeHours[] = {1, 6, 24, 168, 720};
        for (int i = 0; i < 5; ++i) {
            if (i > 0) ImGui::SameLine(0.f, 4.f);
            const bool sel = st.rangeHours == rangeHours[i];
            if (sel) {
                const ImVec4 a = AccentColor();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(a.x, a.y, a.z, 0.45f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(a.x, a.y, a.z, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(a.x, a.y, a.z, 0.65f));
            }
            if (ImGui::Button(l.Get("mon", "main", rangeKeys[i]).c_str())) {
                st.rangeHours = rangeHours[i];
                // Show the whole range that was just picked. Leaving the zoom
                // where it was is why these buttons looked like they did
                // nothing: the window stayed 10 minutes wide either way.
                st.visibleMinutes = MaxVisibleMinutes(st.rangeHours);
                st.timeView.manualX = false;
                InvalidateMonitorCaches(st);
            }
            if (sel) ImGui::PopStyleColor(3);
        }
        ImGui::SameLine(0.f, 10.f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(l.Get("mon", "main", "zoom_short").c_str());
        ImGui::SameLine(0.f, 4.f);
        ImGui::SetNextItemWidth(120.f * UiScale());
        if (ImGui::SliderInt("##mon_zoom", &st.visibleMinutes, 1, maxVisibleMin, "%d min"))
            st.timeView.manualX = false;

        if (!metrics_svc::IsRunningCached()) {
            ImGui::SameLine(0.f, 10.f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", l.Get("mon", "main", "gui_collect_hint").c_str());
        }
        if (st.lastBucketSeconds > 0.0) {
            const std::string bucket = FormatBucketWidth(st.lastBucketSeconds);
            if (!bucket.empty()) {
                ImGui::SameLine(0.f, 10.f);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s %s", l.Get("mon", "main", "avg_per").c_str(),
                                    bucket.c_str());
            }
        }
    }

    PopCompactToolbarStyle();
    ImGui::Dummy(ImVec2(0.f, 2.f * UiScale()));

    const float sidebarW = 188.f * UiScale();
    ImGui::BeginChild("mon_sidebar", ImVec2(sidebarW, 0));
    ImGui::PushItemWidth(-1.f);
    {
        auto cpuSpark = SparkFromSystem(sparkSamples, &metrics::SystemSample::cpuTotal);
        if (cpuSpark.empty() && liveSystem.cpuTotal > 0) cpuSpark.push_back(liveSystem.cpuTotal);
        const std::string cpuSub = std::to_string(static_cast<int>(liveSystem.cpuTotal)) + "%";
        if (DrawResourceTile("##cpu", l.Get("mon", "res", "cpu"), cpuSub, cpuSpark,
                             st.selected == ResourceId::Cpu, dark))
            st.selected = ResourceId::Cpu;

        auto gpuSpark = SparkFromSystem(sparkSamples, &metrics::SystemSample::gpuPct);
        if (gpuSpark.empty() && liveSystem.gpuPct > 0) gpuSpark.push_back(liveSystem.gpuPct);
        const std::string gpuSub = std::to_string(static_cast<int>(liveSystem.gpuPct)) + "%";
        if (DrawResourceTile("##gpu", l.Get("mon", "res", "gpu"), gpuSub, gpuSpark,
                             st.selected == ResourceId::Gpu, dark))
            st.selected = ResourceId::Gpu;

        auto ramSpark = SparkFromSystem(sparkSamples, &metrics::SystemSample::ramPct);
        if (ramSpark.empty() && liveSystem.ramPct > 0) ramSpark.push_back(liveSystem.ramPct);
        const std::string ramSub = std::to_string(static_cast<int>(liveSystem.ramPct)) + "%";
        if (DrawResourceTile("##ram", l.Get("mon", "res", "ram"), ramSub, ramSpark,
                             st.selected == ResourceId::Ram, dark))
            st.selected = ResourceId::Ram;

        auto netSpark = SparkFromNet(sparkSamples);
        const std::string netSub = std::to_string(liveSystem.netDown / 1024) + " KB/s";
        if (DrawResourceTile("##net", l.Get("mon", "res", "net"), netSub, netSpark,
                             st.selected == ResourceId::Net, dark))
            st.selected = ResourceId::Net;

        auto diskSpark = SparkFromSystem(sparkSamples, &metrics::SystemSample::diskPct);
        const std::string diskSub = std::to_string(static_cast<int>(liveSystem.diskPct)) + "%";
        if (DrawResourceTile("##disk", l.Get("mon", "res", "disk"), diskSub, diskSpark,
                             st.selected == ResourceId::Disk, dark))
            st.selected = ResourceId::Disk;
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("mon_detail", ImVec2(0, 0));
    if (st.timeViewResource != st.selected) {
        st.timeView.manualX = false;
        st.timeViewResource = st.selected;
    }
    const bool noChartData = sysSamples.empty() && liveSystem.ts <= 0;
    switch (st.selected) {
    case ResourceId::Cpu: {
        const int prevTopK = settings.cpuAutoTopK;
        DrawCpuVisualizer(l, layout, liveLogical, st.cpuViz, liveSampleKey,
                          liveSystem.ts > 0 ? static_cast<double>(liveSystem.ts)
                                            : static_cast<double>(time(nullptr)),
                          "cpu_plot", visibleSec, st.rangeHours, st.timeView, st.cachedCpuRows,
                          settings.cpuAutoTopK);
        if (settings.cpuAutoTopK != prevTopK) settings.Save();
        break;
    }
    case ResourceId::Gpu:
        if (noChartData) {
            ImGui::TextWrapped("%s", l.Get("mon", "main", "no_data").c_str());
            break;
        }
        DrawSystemGraph(l, sysSamples, &metrics::SystemSample::gpuPct, "gpu_plot",
                        l.Get("mon", "graph", "pct").c_str(), l.Get("mon", "res", "gpu").c_str(),
                        true, visibleSec, st.rangeHours, st.timeView,
                        &st.lastBucketSeconds);
        break;
    case ResourceId::Ram:
        if (noChartData) {
            ImGui::TextWrapped("%s", l.Get("mon", "main", "no_data").c_str());
            break;
        }
        DrawSystemGraph(l, sysSamples, &metrics::SystemSample::ramPct, "ram_plot",
                        l.Get("mon", "graph", "pct").c_str(), l.Get("mon", "res", "ram").c_str(),
                        true, visibleSec, st.rangeHours, st.timeView,
                        &st.lastBucketSeconds);
        break;
    case ResourceId::Net:
        if (noChartData) {
            ImGui::TextWrapped("%s", l.Get("mon", "main", "no_data").c_str());
            break;
        }
        ImGui::Text("%s: %lld B/s", l.Get("mon", "res", "net").c_str(),
                    static_cast<long long>(liveSystem.netDown));
        DrawSystemGraphValues(l, sysSamples, "net_plot", l.Get("mon", "graph", "speed").c_str(),
                              l.Get("mon", "res", "net").c_str(), false, visibleSec, st.rangeHours,
                              st.timeView,
                              [](const metrics::SystemSample& s) {
                                  return static_cast<double>(s.netDown) / 1024.0;
                              },
                              &st.lastBucketSeconds);
        break;
    case ResourceId::Disk:
        if (noChartData) {
            ImGui::TextWrapped("%s", l.Get("mon", "main", "no_data").c_str());
            break;
        }
        DrawSystemGraph(l, sysSamples, &metrics::SystemSample::diskPct, "disk_plot",
                        l.Get("mon", "graph", "pct").c_str(), l.Get("mon", "res", "disk").c_str(),
                        true, visibleSec, st.rangeHours, st.timeView,
                        &st.lastBucketSeconds);
        break;
    }
    ImGui::EndChild();
}

void OnMonitorPageClosed() { InvalidateMonitorCaches(State()); }

} // namespace maku::ui::pages
