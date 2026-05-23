#pragma once
#include "app/Application.h"
#include <string>

namespace maku::cli {

struct Options {
    bool showHelp = false;
    bool runGui = true;
    bool silent = false;
    bool reboot = false;
    bool safeMode = false;
    bool topmost = false;
    bool pciCompact = false;

    bool fetchClip = false;
    bool fetchConsole = false;
    bool disableUpdates = false;
    bool enableUpdates = false;
    bool clearUpdateCache = false;
    bool ttlBypass = false;
    bool debloatUwp = false;
    bool activateHwid = false;
    bool cleanupTemp = false;
    bool restoreRollback = false;

    std::wstring reportPath;
    std::wstring killFromPath;
    std::wstring cleanFolder;
    std::wstring applyPresetPath;
    std::wstring logPath;
    std::wstring killProcess;
    std::wstring blockAdd;
    std::string lang;
    std::string tab;
};

Options Parse(int argc, wchar_t* argv[]);
void PrintHelp();
int RunHeadless(const Options& opts);

app::PageId TabFromAlias(const std::string& tab);

} // namespace maku::cli
