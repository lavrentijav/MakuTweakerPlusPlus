#pragma once
#include <string>

namespace maku::analytics {

// Telemetry can be disabled in Settings (disableTelemetry).
//
// Transparent data sent when enabled:
//   app_launch / app_launch_30sec  -> app language
//   screen_view                      -> screen/tab name (Explorer, Performance, …)
//   benchmark_result                 -> CPU name, score type (single|multi), score value
//
// Endpoints, measurement IDs, signing material and transport keys are not stored as
// readable literals in source; see TelemetryWire.cpp.

bool IsEnabled();
void ScheduleLaunchEvents(const std::string& lang);
void TrackScreen(const std::string& pageTag);
void TrackBenchmark(const std::string& cpuName, const std::string& scoreType,
                    const std::string& score);

} // namespace maku::analytics
