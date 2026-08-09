#pragma once

#include <string>
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

/// Bucket width, in seconds, for a view covering `spanSeconds` at roughly
/// `targetPoints` points. Snapped to a human interval (1s, 5s, 1m, 5m, 1h, …)
/// so the label under the chart reads "1 min" rather than "37 s".
double NiceBucketSeconds(double spanSeconds, int targetPoints);

/// Averages samples into fixed wall-clock buckets. Index-based downsampling
/// distorts irregularly spaced data — with a gap in collection it would stretch
/// the surrounding samples across the hole. Bucketing by time keeps every point
/// at its real position and makes an hour-wide view show one averaged point per
/// minute, which is what the axis labels imply.
///
/// `bucketSeconds` <= 0 copies the input through unchanged.
void DownsampleTimeBuckets(const std::vector<double>& xs, const std::vector<double>& ys,
                           std::vector<double>& outX, std::vector<double>& outY,
                           double bucketSeconds);

/// Human-readable bucket width ("5 s", "1 min", "2 h") for the chart caption.
std::string FormatBucketWidth(double bucketSeconds);

} // namespace maku::ui
