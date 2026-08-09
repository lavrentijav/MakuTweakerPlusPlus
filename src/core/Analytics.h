#pragma once
#include "core/Settings.h"

#include <string>
#include <utility>
#include <vector>

namespace maku::analytics {

// Nothing leaves the machine until the user answers the first-run prompt with
// "allow". Until then GetConsent() is Unknown and every transmit is suppressed.
//
// Transparent data sent when enabled:
//   app_launch / app_launch_30sec  -> app language
//   screen_view                      -> screen/tab name (Explorer, Performance, …)
//   benchmark_result                 -> CPU name, score type (single|multi), score value
//
// Endpoints, measurement IDs, signing material and transport keys are not stored as
// readable literals in source; see TelemetryWire.cpp.

AnalyticsConsent GetConsent();
/// Persists the answer and opens or closes the channel to match.
void SetConsent(AnalyticsConsent consent);

/// True only when consent was explicitly granted.
bool IsEnabled();

void ScheduleLaunchEvents(const std::string& lang);
void TrackScreen(const std::string& pageTag);
void TrackBenchmark(const std::string& cpuName, const std::string& scoreType,
                    const std::string& score);

/// Sends one benchmark result even when analytics are off. Only ever called
/// from the explicit "share this result" button, never automatically.
/// Returns false if the result could not be sent.
bool ShareBenchmarkWithAuthor(const std::string& cpuName, const std::string& scoreType,
                              const std::string& score);

/// Locally counted page visits, most visited first. Recorded regardless of
/// consent so the prompt can show exactly what would be shared, but only
/// transmitted once consent is granted.
std::vector<std::pair<std::string, int>> TopVisitedTabs(size_t limit = 5);

/// Human-readable screen name for a page tag, used by the UI and the prompt.
std::string ScreenName(const std::string& pageTag);

} // namespace maku::analytics
