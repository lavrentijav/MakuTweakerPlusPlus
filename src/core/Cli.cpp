#include "core/Cli.h"
#include "core/Clipboard.h"
#include "core/LogFile.h"
#include "core/ProcessMgrUtil.h"
#include "core/Preset.h"
#include "core/ProcessRunner.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include "core/SystemReport.h"
#include "core/Tweaks.h"
#include "core/Automation.h"
#include "core/Rollback.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <shellapi.h>

namespace maku::cli {
namespace {

bool IsFlag(const std::wstring& a, const wchar_t* longOpt, const wchar_t* shortOpt = nullptr) {
    if (_wcsicmp(a.c_str(), longOpt) == 0) return true;
    return shortOpt && _wcsicmp(a.c_str(), shortOpt) == 0;
}

std::wstring NextPathArg(int& i, const int argc, wchar_t* argv[]) {
    if (i + 1 >= argc) return {};
    return argv[++i];
}

bool InitConsole() {
    if (AttachConsole(ATTACH_PARENT_PROCESS) || AllocConsole()) {
        FILE* out = nullptr;
        FILE* err = nullptr;
        freopen_s(&out, "CONOUT$", "w", stdout);
        freopen_s(&err, "CONOUT$", "w", stderr);
        SetConsoleOutputCP(CP_UTF8);
        return true;
    }
    return false;
}

void PrintWide(const std::wstring& text) {
    const std::string utf8 = util::ToUtf8(text);
    std::cout << utf8;
}

void ScheduleReboot() {
    proc::Run(L"shutdown.exe", L"/r /t 10 /c \"MakuTweaker++: reboot after applying tweaks\"");
}

} // namespace

Options Parse(const int argc, wchar_t* argv[]) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        const std::wstring a = argv[i];
        if (IsFlag(a, L"--help", L"-h") || IsFlag(a, L"/?")) {
            o.showHelp = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--fetch-clip", L"-fc")) {
            o.fetchClip = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--fetch", L"-f")) {
            o.fetchConsole = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--report")) {
            o.reportPath = NextPathArg(i, argc, argv);
            o.runGui = false;
        } else if (IsFlag(a, L"--apply")) {
            o.applyPresetPath = NextPathArg(i, argc, argv);
        } else if (IsFlag(a, L"--silent", L"-s")) {
            o.silent = true;
        } else if (IsFlag(a, L"--reboot")) {
            o.reboot = true;
        } else if (IsFlag(a, L"--disable-updates")) {
            o.disableUpdates = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--enable-updates")) {
            o.enableUpdates = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--clear-update-cache")) {
            o.clearUpdateCache = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--ttl-bypass")) {
            o.ttlBypass = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--debloat-uwp")) {
            o.debloatUwp = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--activate-hwid")) {
            o.activateHwid = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--block-add")) {
            o.blockAdd = NextPathArg(i, argc, argv);
        } else if (IsFlag(a, L"--kill")) {
            o.killProcess = NextPathArg(i, argc, argv);
        } else if (IsFlag(a, L"--topmost")) {
            o.topmost = true;
        } else if (IsFlag(a, L"--tab")) {
            o.tab = util::ToUtf8(NextPathArg(i, argc, argv));
        } else if (IsFlag(a, L"--compact")) {
            o.pciCompact = true;
        } else if (IsFlag(a, L"--lang")) {
            o.lang = util::ToUtf8(NextPathArg(i, argc, argv));
        } else if (IsFlag(a, L"--safe-mode")) {
            o.safeMode = true;
        } else if (IsFlag(a, L"--log")) {
            o.logPath = NextPathArg(i, argc, argv);
        } else if (IsFlag(a, L"--cleanup-temp")) {
            o.cleanupTemp = true;
            o.runGui = false;
        } else if (IsFlag(a, L"--kill-from-path")) {
            o.killFromPath = NextPathArg(i, argc, argv);
        } else if (IsFlag(a, L"--clean-folder")) {
            o.cleanFolder = NextPathArg(i, argc, argv);
            o.runGui = false;
        } else if (IsFlag(a, L"--restore-rollback")) {
            o.restoreRollback = true;
            o.runGui = false;
        }
    }

    if (o.silent && !o.applyPresetPath.empty()) o.runGui = false;
    if (o.showHelp) o.runGui = false;
    return o;
}

void PrintHelp() {
    InitConsole();
    const char* h = R"(MakuTweaker++ 5.6 CLI

Info:
  --fetch-clip, -fc          Copy compact PC summary to clipboard and exit
  --fetch, -f                Print fastfetch-style info to console
  --report <path>            Write full system report to file

Presets:
  --apply <file.mktw>        Apply preset (JSON)
  --silent, -s               With --apply: no GUI
  --reboot                   Reboot in 10s after headless apply

Tweaks:
  --disable-updates          Disable Windows Update
  --enable-updates           Re-enable Windows Update
  --clear-update-cache       Clear SoftwareDistribution cache
  --ttl-bypass               Set TTL to 65
  --debloat-uwp              Remove common bundled UWP apps

Processes:
  --kill <name.exe>          Kill all processes with this name
  --block-add "a.exe,b.exe"  Append to app block list (makuYanPar)

GUI:
  --tab <name>               tweaks|processes|uwp|updates|info|advanced
  --compact                  Compact PC info tab
  --lang ru|en               Force UI language
  --topmost                  Always on top
  --safe-mode                Ignore saved exclusions on launch
  --log <path>               Log registry/tweak operations

  --help, -h                 This help
)";
    std::cout << h;
}

app::PageId TabFromAlias(const std::string& tab) {
    if (tab == "tweaks" || tab == "quick") return app::PageId::QuickSetup;
    if (tab == "processes" || tab == "procmgr" || tab == "pmgr") return app::PageId::ProcessMgr;
    if (tab == "uwp") return app::PageId::Uwp;
    if (tab == "updates" || tab == "wu") return app::PageId::WindowsUpdate;
    if (tab == "info" || tab == "pci") return app::PageId::Pci;
    if (tab == "advanced" || tab == "adv") return app::PageId::Advanced;
    return app::PageFromTag(tab);
}

int RunHeadless(const Options& opts) {
    if (!opts.logPath.empty())
        logf::Init(opts.logPath);
    else if (!logf::Active())
        logf::InitDefault();

    if (opts.showHelp) {
        PrintHelp();
        return 0;
    }

    if (opts.activateHwid) {
        InitConsole();
        std::cout << "HWID activation is not available in the open-source build.\n";
        return 1;
    }

    if (opts.fetchClip) {
        const std::wstring text = report::BuildCompactSummary();
        return clip::SetText(text) ? 0 : 1;
    }

    if (opts.fetchConsole) {
        InitConsole();
        std::cout << report::BuildFastfetchArt();
        PrintWide(report::BuildCompactSummary());
        return 0;
    }

    if (!opts.reportPath.empty()) {
        std::wofstream out(opts.reportPath);
        if (!out) return 1;
        out << report::BuildFullReport();
        return 0;
    }

    if (opts.disableUpdates) {
        tweaks::SetWindowsUpdatesEnabled(false);
        if (opts.reboot) ScheduleReboot();
        return 0;
    }
    if (opts.enableUpdates) {
        tweaks::SetWindowsUpdatesEnabled(true);
        return 0;
    }
    if (opts.clearUpdateCache) {
        tweaks::ClearWindowsUpdateCache();
        if (opts.reboot) ScheduleReboot();
        return 0;
    }
    if (opts.ttlBypass) {
        tweaks::ApplyTtlBypass();
        return 0;
    }
    if (opts.debloatUwp) {
        tweaks::DebloatUwp();
        return 0;
    }

    if (opts.cleanupTemp) {
        auto_task::RunTempCleanup();
        return 0;
    }

    if (opts.restoreRollback) {
        return rollback::RestoreLatest() ? 0 : 1;
    }

    if (!opts.killFromPath.empty()) {
        pmgr::KillProcessesByImagePath(opts.killFromPath);
        return 0;
    }

    if (!opts.cleanFolder.empty()) {
        wchar_t cmd[512];
        swprintf_s(cmd, L"/c rd /s /q \"%s\"", opts.cleanFolder.c_str());
        proc::Run(L"cmd.exe", cmd);
        return 0;
    }

    if (!opts.applyPresetPath.empty()) {
        Settings s;
        s.Load();
        const auto r = preset::ApplyFile(opts.applyPresetPath, s, opts.safeMode);
        if (opts.reboot && r.needsReboot) ScheduleReboot();
        if (opts.silent) return r.ok ? 0 : 1;
    }

    return -1;
}

} // namespace maku::cli
