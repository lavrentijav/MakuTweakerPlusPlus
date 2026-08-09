#include "app/Application.h"
#include "app/Branding.h"
#include "cli/Cli.h"
#include "core/OsUtil.h"
#include "ui/Dpi.h"
#include "platform/MetricsService.h"
#include "platform/MetricsTray.h"
#include <shellapi.h>
#include <windows.h>

static bool HasArg(int argc, wchar_t** argv, const wchar_t* flag) {
    for (int i = 1; i < argc; ++i)
        if (_wcsicmp(argv[i], flag) == 0) return true;
    return false;
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmdLine, int) {
    maku::ui::EnablePerMonitorDpi();

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    if (HasArg(argc, argv, L"--metrics-service")) {
        LocalFree(argv);
        return maku::metrics_svc::RunServiceMain();
    }
    if (HasArg(argc, argv, L"--metrics-tray")) {
        const bool openGui = HasArg(argc, argv, L"--open-gui");
        LocalFree(argv);
        return maku::metrics_tray::RunStandalone(hInst, openGui);
    }
    if (HasArg(argc, argv, L"--install-metrics-service")) {
        LocalFree(argv);
        return maku::metrics_svc::InstallElevated() ? 0 : 1;
    }
    if (HasArg(argc, argv, L"--uninstall-metrics-service")) {
        LocalFree(argv);
        return maku::metrics_svc::Uninstall() ? 0 : 1;
    }

    // Anything that is not a legacy GUI switch runs headless.
    if (maku::cli::WantsCli(argc, argv)) {
        const int code = maku::cli::Run(argc, argv);
        LocalFree(argv);
        return code;
    }
    LocalFree(argv);

    if (!maku::os::IsWindowsSupported()) {
        MessageBoxW(nullptr,
                    L"Your version of Windows is not supported. Update to Windows 10 1607 or higher.",
                    maku::brand::kDisplayName, MB_OK | MB_ICONERROR);
        return 1;
    }
    auto& app = maku::app::Application::Instance();
    if (!app.Init(hInst, cmdLine)) return 1;
    app.Run();
    app.Shutdown();
    return 0;
}
