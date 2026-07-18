#include "ui/MonitorPlotAxis.h"
#include <imgui.h>
#include <implot.h>
#include <algorithm>

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

} // namespace maku::ui
