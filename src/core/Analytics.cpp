#include "core/Analytics.h"
#include "core/Settings.h"
#include "core/TelemetryWire.h"

#include <algorithm>
#include <chrono>
#include <mutex>
#include <thread>

namespace maku::analytics {
namespace {

// User-visible payload (see Settings -> telemetryabt in loc/):
//   Events: app_launch, app_launch_30sec, screen_view, benchmark_result
//   Fields: app language, screen/tab name, CPU name, benchmark score type & value
// Wire encoding (endpoint, measurement id, salts, query keys) lives in TelemetryWire.cpp.

std::mutex g_mutex;

/// Settings on disk are the source of truth, but TrackScreen runs on every page
/// switch and a full load/save round trip per click is wasteful. Keep the
/// counters in memory and flush them on a change.
Settings LoadSettings() {
    Settings s;
    s.Load();
    return s;
}

} // namespace

std::string ScreenName(const std::string& tag) {
    if (tag == "exp") return "Explorer";
    if (tag == "wu") return "Windows Update";
    if (tag == "sys") return "System and Recovery";
    if (tag == "per") return "Personalization";
    if (tag == "uwp") return "UWP Apps";
    if (tag == "quick") return "Quick Settings";
    if (tag == "adv") return "Advanced Settings";
    if (tag == "compon") return "Windows Components";
    if (tag == "act") return "Windows Activation";
    if (tag == "perf") return "Performance";
    if (tag == "sat") return "Shutdown Timer";
    if (tag == "pmgr") return "Task Manager";
    if (tag == "pci") return "PC Information";
    if (tag == "wininfo") return "Windows Information";
    if (tag == "mon") return "Monitoring";
    if (tag == "settings") return "Settings";
    return "Unknown Tab";
}

AnalyticsConsent GetConsent() {
    const Settings s = LoadSettings();
    switch (s.analyticsConsent) {
    case static_cast<int>(AnalyticsConsent::Granted): return AnalyticsConsent::Granted;
    case static_cast<int>(AnalyticsConsent::Declined): return AnalyticsConsent::Declined;
    default: return AnalyticsConsent::Unknown;
    }
}

void SetConsent(AnalyticsConsent consent) {
    std::lock_guard lock(g_mutex);
    Settings s = LoadSettings();
    s.analyticsConsent = static_cast<int>(consent);
    s.disableTelemetry = consent != AnalyticsConsent::Granted;
    s.Save();
}

bool IsEnabled() { return GetConsent() == AnalyticsConsent::Granted; }

void ScheduleLaunchEvents(const std::string& lang) {
    if (!IsEnabled() || !wire::ChannelOpen()) return;
    std::thread([lang] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        wire::Transmit(wire::Ev::Launch, {{wire::Fd::Lang, lang}});
        std::this_thread::sleep_for(std::chrono::seconds(28));
        wire::Transmit(wire::Ev::Launch30, {{wire::Fd::Lang, lang}});
    }).detach();
}

void TrackScreen(const std::string& pageTag) {
    {
        // Counted even when analytics are off: the consent dialog and the
        // settings page show these numbers, and the user is entitled to see
        // what they would be sharing before they agree to share it.
        std::lock_guard lock(g_mutex);
        Settings s = LoadSettings();
        ++s.tabVisits[pageTag];
        s.Save();
    }
    if (!IsEnabled()) return;
    wire::Transmit(wire::Ev::Screen, {{wire::Fd::Screen, ScreenName(pageTag)}});
}

void TrackBenchmark(const std::string& cpuName, const std::string& scoreType,
                    const std::string& score) {
    if (!IsEnabled() || !wire::ChannelOpen()) return;
    wire::Transmit(wire::Ev::Bench, {{wire::Fd::Cpu, cpuName},
                                     {wire::Fd::ScoreType, scoreType},
                                     {wire::Fd::Score, score}});
}

bool ShareBenchmarkWithAuthor(const std::string& cpuName, const std::string& scoreType,
                              const std::string& score) {
    if (cpuName.empty() || score.empty()) return false;
    return wire::TransmitOnce(wire::Ev::Bench, {{wire::Fd::Cpu, cpuName},
                                                {wire::Fd::ScoreType, scoreType},
                                                {wire::Fd::Score, score}});
}

std::vector<std::pair<std::string, int>> TopVisitedTabs(size_t limit) {
    const Settings s = LoadSettings();
    std::vector<std::pair<std::string, int>> out(s.tabVisits.begin(), s.tabVisits.end());
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
        if (a.second != b.second) return a.second > b.second;
        return a.first < b.first;
    });
    if (out.size() > limit) out.resize(limit);
    return out;
}

} // namespace maku::analytics
