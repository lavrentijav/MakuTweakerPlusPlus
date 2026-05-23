#include "app/Application.h"
#include "app/Branding.h"
#include "core/Automation.h"
#include "core/JobQueue.h"
#include "core/OsUtil.h"
#include "core/ProcessWatchdog.h"
#include "core/ProcessRunner.h"
#include "core/LogFile.h"
#include "core/PendingUi.h"
#include "core/Rollback.h"
#include "core/StringUtil.h"
#include "platform/CrashHandler.h"
#include "platform/UpdateChecker.h"
#include "platform/Win32Window.h"
#include "ui/AppShell.h"
#include "ui/Theme.h"
#include "ui/Fonts.h"
#include <fstream>
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include "core/PciCollector.h"
#include "platform/D3D11Context.h"
#include "platform/WinAppSdkRuntime.h"

#include <cstdio>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace maku::app {

static Application* g_app = nullptr;
static platform::D3D11Context g_dx;
static platform::Win32Window g_window;

Application& Application::Instance() {
    static Application app;
    return app;
}

bool Application::Init(HINSTANCE inst) {
    g_app = this;
    platform::InstallCrashHandler();
    if (!logf::Active()) logf::InitDefault();
    logf::Info(os::HasAdminRights() ? "Process has administrator rights"
                                    : "Process running without UAC elevation");
    settings_.Load();
    pending::Load();
    settings_.theme = ui::NormalizeTheme(settings_.theme);
    if (launch_.safeMode) settings_.processExclusions.clear();
    if (launch_.safeMode) settings_.makuYanPar.clear();
    if (settings_.firRun) {
        settings_.lang = Settings::DetectSystemLanguage();
        settings_.firRun = false;
        settings_.theme = "Dark";
        settings_.Save();
    }
    l10n_ = std::make_unique<l10n::Localization>(settings_.lang);

    if (!g_window.Create(inst, brand::kDisplayName)) return false;
    hwnd_ = g_window.Hwnd();
    os::SetElevationOwner(hwnd_);

    if (!g_dx.Init(hwnd_)) return false;

    RECT client{};
    GetClientRect(hwnd_, &client);
    g_dx.Resize(static_cast<UINT>(client.right - client.left),
                static_cast<UINT>(client.bottom - client.top));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ui::InitFonts(hwnd_);
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(g_dx.Device(), g_dx.Context());
    ImGui_ImplDX11_CreateDeviceObjects();
    ui::ApplyTheme(settings_.theme);
    ui::SyncDwmTheme(hwnd_, settings_.theme, g_dx.UsesAlphaSwapChain());
    ui::InitAppShell();
    pci::PciCollector::Instance().Prefetch();
    tray_.Create(hwnd_, inst);

    currentPage_ = launch_.startPage != PageId::Count ? launch_.startPage
                                                      : PageFromTag(settings_.lastPageTag);
    if (currentPage_ == PageId::Count) currentPage_ = PageId::Explorer;

    if (launch_.topmost)
        SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

#ifndef MAKUTWEAKER_BUILD
#define MAKUTWEAKER_BUILD 0
#endif
    if (!settings_.disableUpdateNotify) {
        platform::CheckForUpdateAsync(MAKUTWEAKER_BUILD, [this](platform::UpdateInfo info) {
            if (info.available) {
                pendingUpdate_ = info;
                showUpdateDialog_ = true;
            }
        });
    }

    watchdog::Start();
    watchdog::SetEnabled(settings_.processWatchdog);
    if (os::IsElevated()) pending::ApplyMiscAfterElevation(settings_);
    if (settings_.autoTtlOnBoot && !auto_task::IsBootTtlInstalled()) auto_task::InstallBootTtl();
    if (settings_.autoWeeklyCleanup && !auto_task::IsWeeklyCleanupInstalled())
        auto_task::InstallWeeklyCleanup();

    g_window.SetMessageHandler([](UINT msg, WPARAM wp, LPARAM lp) {
        if (ImGui_ImplWin32_WndProcHandler(g_app->hwnd_, msg, wp, lp)) return;
        if (msg == WM_SIZE && wp != SIZE_MINIMIZED) {
            g_dx.Resize(LOWORD(lp), HIWORD(lp));
        }
    });
    return true;
}

void Application::RequestFontReload() { pendingFontReload_ = true; }

void Application::ReloadLanguage() {
    l10n_ = std::make_unique<l10n::Localization>(settings_.lang);
    RequestFontReload();
    ui::InitAppShell();
}

void Application::NotifyReboot(int mode) {
    rollback::SnapshotBeforeReboot();
    auto& l = L10n();
    std::wstring msg;
    if (mode == 1) msg = util::ToWide(l.Def("rebnotify"));
    else if (mode == 2) msg = util::ToWide(l.Def("rebnotifyexplorer"));
    else msg = util::ToWide(l.Def("rebnotifysfc"));
    tray_.ShowBalloon(brand::kDisplayName, msg);
}

void Application::RequestExplorerRestart() {
    logf::Info("Explorer restart requested");
    explorerRestartPending_ = true;
    explorerRestartAt_ = GetTickCount() + 1500;
    proc::KillExplorer();
}

const char* Application::RendererGlassInfo() const {
    static char buf[192];
    snprintf(buf, sizeof(buf), "%s | %s", g_dx.GlassSwapModeName(), ui::DwmGlassModeName());
    return buf;
}

void Application::RefreshGlassPipeline() {
    ui::SyncDwmTheme(hwnd_, settings_.theme, g_dx.UsesAlphaSwapChain());
}

void Application::SetPage(PageId page) {
    currentPage_ = page;
    if (page != PageId::Settings) {
        settings_.lastPageTag = PageTag(page);
        settings_.Save();
    }
}

void Application::Run() {
    while (g_window.ProcessMessages()) {
        if (explorerRestartPending_ && GetTickCount() >= explorerRestartAt_) {
            proc::StartExplorer();
            explorerRestartPending_ = false;
        }
        if (pendingFontReload_) {
            pendingFontReload_ = false;
            ui::ReloadFonts(hwnd_);
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ui::DrawAppShell();
        ImGui::Render();
        float clear[4]{};
        ui::FrameClearColor(settings_.theme, g_dx.UsesAlphaSwapChain(), clear);
        g_dx.BeginFrame(clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_dx.EndFrame();
    }
}

void Application::Shutdown() {
    logf::Info("MakuTweaker++ session ended");
    pci::PciCollector::Instance().Shutdown();
    watchdog::Stop();
    jobs::JobQueue::Instance().Shutdown();
    tray_.Destroy();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_dx.Shutdown();
}

} // namespace maku::app
