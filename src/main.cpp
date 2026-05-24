#include "app/Application.h"
#include "app/Branding.h"
#include "platform/WinAppSdkRuntime.h"
#include "core/Cli.h"
#include "core/LogFile.h"
#include "core/OsUtil.h"
#include "core/Preset.h"
#include "core/ProcessMgrUtil.h"
#include "core/StringUtil.h"
#include "core/Settings.h"
#include "platform/MetricsService.h"
#include "platform/MetricsTray.h"
#include <shellapi.h>
#include <windows.h>

static void AppendBlockList(maku::Settings& s, const std::wstring& csvWide) {
    const std::string csv = maku::util::ToUtf8(csvWide);
    if (csv.empty()) return;
    if (!s.makuYanPar.empty()) s.makuYanPar += ", ";
    s.makuYanPar += csv;
    s.Save();
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    const maku::cli::Options opts = maku::cli::Parse(argc, argv);

    if (opts.metricsService) {
        LocalFree(argv);
        return maku::metrics_svc::RunServiceMain();
    }
    if (opts.metricsTray) {
        LocalFree(argv);
        return maku::metrics_tray::RunStandalone(hInst, opts.metricsTrayOpenGui);
    }

    if (!opts.logPath.empty())
        maku::logf::Init(opts.logPath);
    else
        maku::logf::InitDefault();

    const int headlessCode = maku::cli::RunHeadless(opts);
    LocalFree(argv);
    if (headlessCode >= 0) return headlessCode;

    if (!opts.foregroundGui && !opts.metricsTray && !opts.metricsService) {
        maku::Settings settings;
        settings.Load();
        if (settings.metricsServiceEnabled && maku::metrics_svc::IsRunning()) {
            // Service owns metrics; GUI always runs in a separate process.
            maku::metrics_tray::EnsureStandaloneTray();
            std::wstring guiArgs = L"--foreground-gui";
            if (!opts.tab.empty()) guiArgs += L" --tab " + maku::util::ToWide(opts.tab);
            maku::metrics_tray::RequestOpenMainGui(guiArgs);
            return 0;
        }
    }

    if (!maku::os::IsWindowsSupported()) {
        MessageBoxW(nullptr,
                    L"Your version of Windows is not supported. Update to Windows 10 1607 or higher.",
                    maku::brand::kDisplayName, MB_OK | MB_ICONERROR);
        return 1;
    }

    auto& app = maku::app::Application::Instance();

    maku::app::Application::LaunchConfig launch;
    launch.topmost = opts.topmost;
    launch.pciCompact = opts.pciCompact;
    launch.safeMode = opts.safeMode;
    if (!opts.tab.empty()) launch.startPage = maku::cli::TabFromAlias(opts.tab);
    app.ConfigureLaunch(launch);

    if (opts.lang.empty() == false) launch.pendingLang = opts.lang;

    if (!opts.killProcess.empty()) {
        std::wstring name = opts.killProcess;
        if (name.find(L".exe") == std::wstring::npos) name += L".exe";
        maku::pmgr::KillProcessesByName(name);
    }
    if (!opts.killFromPath.empty()) maku::pmgr::KillProcessesByImagePath(opts.killFromPath);

    if (!opts.blockAdd.empty()) {
        auto& s = app.GetSettings();
        s.Load();
        AppendBlockList(s, opts.blockAdd);
    }

    if (!opts.applyPresetPath.empty() && !opts.silent) {
        auto& s = app.GetSettings();
        s.Load();
        maku::preset::ApplyFile(opts.applyPresetPath, s, opts.safeMode);
    }

    maku::platform::WinAppSdkRuntime::Initialize();

    if (!app.Init(hInst)) {
        maku::platform::WinAppSdkRuntime::Shutdown();
        return 1;
    }
    app.Run();
    app.Shutdown();
    maku::platform::WinAppSdkRuntime::Shutdown();
    return 0;
}
