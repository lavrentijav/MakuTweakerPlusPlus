#include "core/Analytics.h"
#include "core/Settings.h"
#include "core/TelemetryWire.h"
#include <chrono>
#include <thread>

namespace maku::analytics {
namespace {

// User-visible payload (see Settings -> telemetryabt in loc/):
//   Events: app_launch, app_launch_30sec, screen_view, benchmark_result
//   Fields: app language, screen/tab name, CPU name, benchmark score type & value
// Wire encoding (endpoint, measurement id, salts, query keys) lives in TelemetryWire.cpp.

const char* ScreenNameFromTag(const std::string& tag) {
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

} // namespace

bool IsEnabled() {
    Settings s;
    s.Load();
    return !s.disableTelemetry;
}

void ScheduleLaunchEvents(const std::string& lang) {
    if (!wire::ChannelOpen()) return;
    std::thread([lang] {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        wire::Transmit(wire::Ev::Launch, {{wire::Fd::Lang, lang}});
        std::this_thread::sleep_for(std::chrono::seconds(28));
        wire::Transmit(wire::Ev::Launch30, {{wire::Fd::Lang, lang}});
    }).detach();
}

void TrackScreen(const std::string& pageTag) {
    wire::Transmit(wire::Ev::Screen,
                  {{wire::Fd::Screen, ScreenNameFromTag(pageTag)}});
}

void TrackBenchmark(const std::string& cpuName, const std::string& scoreType,
                    const std::string& score) {
    if (!wire::ChannelOpen()) return;
    wire::Transmit(wire::Ev::Bench,
                  {{wire::Fd::Cpu, cpuName},
                   {wire::Fd::ScoreType, scoreType},
                   {wire::Fd::Score, score}});
}

} // namespace maku::analytics
