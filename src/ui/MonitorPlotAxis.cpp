#include "ui/MonitorPlotAxis.h"
#include <imgui.h>
#include <implot.h>
#include <algorithm>
#include <cmath>
#include <iterator>

namespace maku::ui {

void SetupMonitorTimeAxisX(MonitorTimeAxisView& view, double dataXMin, double dataXMax,
                           int visibleSeconds, int rangeHours) {
    if (view.appliedVisibleSec != visibleSeconds || view.appliedRangeHours != rangeHours) {
        view.manualX = false;
        view.appliedVisibleSec = visibleSeconds;
        view.appliedRangeHours = rangeHours;
    }

    const int span = std::max(30, visibleSeconds);
    const double xMax = dataXMax;
    const double xMin = std::max(dataXMin, xMax - static_cast<double>(span));

    if (!view.manualX)
        ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax + 2.0, ImPlotCond_Always);
    else
        ImPlot::SetupAxisLimits(ImAxis_X1, view.savedXMin, view.savedXMax, ImPlotCond_Always);
}

void UpdateMonitorTimeAxisX(MonitorTimeAxisView& view) {
    const ImPlotRect lim = ImPlot::GetPlotLimits(ImAxis_X1, ImAxis_Y1);
    view.savedXMin = lim.X.Min;
    view.savedXMax = lim.X.Max;

    if (!ImPlot::IsPlotHovered()) return;

    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
        ImGui::IsMouseDragging(ImGuiMouseButton_Middle) || io.MouseWheel != 0.f ||
        io.MouseWheelH != 0.f)
        view.manualX = true;
}

int PlotDownsampleBudget() {
    const ImVec2 size = ImPlot::GetPlotSize();
    return std::clamp(static_cast<int>(size.x * 1.5f), 64, 2048);
}

void FilterSeriesByTime(const std::vector<double>& xs, const std::vector<double>& ys, double xMin,
                        double xMax, std::vector<double>& outX, std::vector<double>& outY) {
    outX.clear();
    outY.clear();
    if (xs.empty() || xs.size() != ys.size()) return;

    const auto begin =
        std::lower_bound(xs.begin(), xs.end(), xMin, [](double t, double v) { return t < v; });
    const auto end =
        std::upper_bound(begin, xs.end(), xMax, [](double v, double t) { return v < t; });
    if (begin == end) {
        outX.push_back(xs.back());
        outY.push_back(ys.back());
        return;
    }
    outX.assign(begin, end);
    outY.assign(ys.begin() + (begin - xs.begin()), ys.begin() + (end - xs.begin()));
}

void DownsampleAverage(const std::vector<double>& xs, const std::vector<double>& ys,
                       std::vector<double>& outX, std::vector<double>& outY, int maxPoints) {
    const size_t n = xs.size();
    if (n == 0 || n != ys.size()) {
        outX.clear();
        outY.clear();
        return;
    }
    if (maxPoints < 2 || static_cast<int>(n) <= maxPoints) {
        outX = xs;
        outY = ys;
        return;
    }

    const size_t buckets = static_cast<size_t>(maxPoints);
    outX.clear();
    outY.clear();
    outX.reserve(buckets);
    outY.reserve(buckets);

    for (size_t b = 0; b < buckets; ++b) {
        const size_t i0 = b * n / buckets;
        const size_t i1 = (b + 1) * n / buckets;
        if (i1 <= i0) continue;
        double sumX = 0.0;
        double sumY = 0.0;
        for (size_t i = i0; i < i1; ++i) {
            sumX += xs[i];
            sumY += ys[i];
        }
        const double count = static_cast<double>(i1 - i0);
        outX.push_back(sumX / count);
        outY.push_back(sumY / count);
    }
}

double NiceBucketSeconds(const double spanSeconds, const int targetPoints) {
    if (spanSeconds <= 0.0 || targetPoints <= 1) return 0.0;

    // Intervals a person would name out loud, from one second to one day.
    static const double kSteps[] = {1,    2,    5,    10,   15,   30,    60,   120,
                                    300,  600,  900,  1800, 3600, 7200,  10800, 21600,
                                    43200, 86400};
    const double ideal = spanSeconds / static_cast<double>(targetPoints);
    for (const double step : kSteps)
        if (step >= ideal) return step;
    return kSteps[std::size(kSteps) - 1];
}

void DownsampleTimeBuckets(const std::vector<double>& xs, const std::vector<double>& ys,
                           std::vector<double>& outX, std::vector<double>& outY,
                           const double bucketSeconds) {
    outX.clear();
    outY.clear();
    const size_t n = xs.size();
    if (n == 0 || n != ys.size()) return;
    if (bucketSeconds <= 0.0) {
        outX = xs;
        outY = ys;
        return;
    }

    outX.reserve(n);
    outY.reserve(n);

    // Buckets are aligned to absolute time, not to the first sample, so the
    // points do not shift sideways as new data arrives.
    double bucketStart = std::floor(xs[0] / bucketSeconds) * bucketSeconds;
    double sumX = 0.0;
    double sumY = 0.0;
    size_t count = 0;

    const auto flush = [&] {
        if (count == 0) return;
        outX.push_back(sumX / static_cast<double>(count));
        outY.push_back(sumY / static_cast<double>(count));
        sumX = 0.0;
        sumY = 0.0;
        count = 0;
    };

    for (size_t i = 0; i < n; ++i) {
        if (xs[i] >= bucketStart + bucketSeconds) {
            flush();
            bucketStart = std::floor(xs[i] / bucketSeconds) * bucketSeconds;
        }
        sumX += xs[i];
        sumY += ys[i];
        ++count;
    }
    flush();
}

std::string FormatBucketWidth(const double bucketSeconds) {
    if (bucketSeconds < 1.0) return {};
    const long long s = static_cast<long long>(bucketSeconds + 0.5);
    if (s < 60) return std::to_string(s) + " s";
    if (s < 3600) return std::to_string(s / 60) + " min";
    if (s < 86400) return std::to_string(s / 3600) + " h";
    return std::to_string(s / 86400) + " d";
}

} // namespace maku::ui
