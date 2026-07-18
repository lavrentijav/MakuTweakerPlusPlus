#pragma once

#include <vector>

namespace maku::ui {

struct MonitorTimeAxisView {
    bool manualX = false;
    int appliedVisibleSec = 0;
    int appliedRangeHours = 0;
    double savedXMin = 0.0;
    double savedXMax = 0.0;
};

void SetupMonitorTimeAxisX(MonitorTimeAxisView& view, double dataXMin, double dataXMax,
                           int visibleSeconds, int rangeHours);
void UpdateMonitorTimeAxisX(MonitorTimeAxisView& view);

int PlotDownsampleBudget();
void FilterSeriesByTime(const std::vector<double>& xs, const std::vector<double>& ys, double xMin,
                        double xMax, std::vector<double>& outX, std::vector<double>& outY);
void DownsampleAverage(const std::vector<double>& xs, const std::vector<double>& ys,
                       std::vector<double>& outX, std::vector<double>& outY, int maxPoints);

} // namespace maku::ui
