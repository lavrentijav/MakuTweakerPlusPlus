#include "app/Application.h"
#include "core/OsUtil.h"

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR cmdLine, int) {
    if (!maku::os::IsWindowsSupported()) {
        MessageBoxW(nullptr,
                    L"Your version of Windows is not supported. Update to Windows 10 1607 or higher.",
                    L"MakuTweaker", MB_OK | MB_ICONERROR);
        return 1;
    }
    auto& app = maku::app::Application::Instance();
    if (!app.Init(hInst, cmdLine)) return 1;
    app.Run();
    app.Shutdown();
    return 0;
}
