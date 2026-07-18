#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace maku {

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
    bool disableTelemetry = false;
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
    int monitoringRefreshMs = 1000;
    int cpuAutoTopK = 8;
    bool pmgrShowMonitoring = false;
    bool metricsServiceEnabled = false;
    int metricsIntervalSec = 5;

    void Load();
    void Save() const;
    std::wstring Path() const;

    static std::string DetectSystemLanguage();
};

} // namespace maku
