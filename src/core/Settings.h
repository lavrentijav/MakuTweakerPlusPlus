#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace maku {

struct Settings {
    std::string lang = "en";
    int langSI = 0;
    bool firRun = true;
    std::string theme = "Dark";
    std::string style = "Tabbed";
    std::string processExclusions;
    std::string makuYanPar;
    std::string lastPageTag = "exp";
    int updIgnoredCount = 0;
    bool disableUpdateNotify = false;
    int lastProcessFilterIndex = 3;
    bool group = false;
    bool compact = false;
    bool onlyFrozen = false;
    bool showSystemProcesses = false;
    bool autoTtlOnBoot = false;
    bool autoWeeklyCleanup = false;
    bool processWatchdog = false;
    bool shellContextMenu = false;
    bool gamingTcp = false;
    std::string preferredDns;

    void Load();
    void Save() const;
    std::wstring Path() const;

    static std::string DetectSystemLanguage();
};

} // namespace maku
