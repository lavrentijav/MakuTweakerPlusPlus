#pragma once
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace maku {

/// Whether the user has answered the first-run analytics question.
enum class AnalyticsConsent : int {
    Unknown = 0, ///< Never asked; show the first-run dialog.
    Granted = 1,
    Declined = 2,
};

struct Settings {
    std::string lang = "en";
    int langSI = 0;
    bool firRun = true;
    std::string theme = "Light";
    std::string style = "Tabbed";
    std::string processExclusions;
    std::string makuYanPar;
    std::string lastPageTag = "exp";
    int updIgnoredCount = 0;
    bool disableUpdateNotify = false;

    /// Effective analytics switch. Kept in sync with `analyticsConsent` — the
    /// transmit path reads this one, so a declined or unanswered prompt leaves
    /// the channel closed.
    bool disableTelemetry = true;
    int analyticsConsent = static_cast<int>(AnalyticsConsent::Unknown);

    /// Page tag -> number of times the user opened it. Aggregated locally so
    /// the "most visited tabs" figure can be shown before anything is sent.
    std::map<std::string, int> tabVisits;

    int lastProcessFilterIndex = 3;
    bool group = false;
    bool compact = false;
    bool onlyFrozen = false;
    bool exclusiveMode = false;
    bool autoStartExclusive = false;
    bool replaceTaskMgr = false;
    bool showSystemProcs = false;
    bool ramMbOnly = false;
    int processViewMode = 0;
    int monitoringRefreshMs = 500;
    int cpuAutoTopK = 8;
    bool pmgrShowMonitoring = false;
    bool metricsServiceEnabled = false;
    int metricsIntervalSec = 2;

    void Load();
    void Save() const;
    std::wstring Path() const;

    static std::string DetectSystemLanguage();
};

} // namespace maku
