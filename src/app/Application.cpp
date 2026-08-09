#include "app/Application.h"
#include "app/Branding.h"
#include "app/Version.h"
#include "core/Analytics.h"
#include "core/EmbeddedStore.h"
#include "core/MetricsBackground.h"
#include "core/OsUtil.h"
#include "core/PresetIO.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include "platform/CrashHandler.h"
#include "platform/MetricsService.h"
#include "platform/MetricsTray.h"
#include "platform/UpdateChecker.h"
#include "platform/Win32Window.h"
#include "ui/AppShell.h"
#include "ui/Modals.h"
#include "ui/MonitorPage.h"
#include "ui/Theme.h"
#include "ui/Fonts.h"
#include "ui/Dpi.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <implot.h>
#include "platform/D3D11Context.h"
#include <atomic>
#include <chrono>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace maku::app {

static Application* g_app = nullptr;
static platform::D3D11Context g_dx;
static platform::Win32Window g_window;

Application& Application::Instance() {
    static Application app;
    return app;
}

void Application::ParseStartupPage(const wchar_t* cmdLine) {
    if (!cmdLine || !*cmdLine) return;
    std::wstring args = cmdLine;
    for (auto& c : args) c = towlower(c);
    auto takeTag = [&](const wchar_t* flag, const char* tag) {
        if (args.find(flag) != std::wstring::npos) {
            currentPage_ = PageFromTag(tag);
            if (std::string(tag) == "mgr") {
                settings_.exclusiveMode = true;
                currentPage_ = PageId::ProcessMgr;
            }
        }
    };
    takeTag(L"/u", "wu");
    takeTag(L"-u", "wu");
    takeTag(L"/p", "perf");
    takeTag(L"-p", "perf");
    takeTag(L"/s", "sat");
    takeTag(L"-s", "sat");
    takeTag(L"/mgr", "mgr");
    takeTag(L"-mgr", "mgr");
    takeTag(L"/pc", "pci");
    takeTag(L"-pc", "pci");
    takeTag(L"/win", "wininfo");
    takeTag(L"-win", "wininfo");
    takeTag(L"/mon", "mon");
    takeTag(L"-mon", "mon");
    const auto mktwPos = args.find(L".mktw");
    if (mktwPos != std::wstring::npos) {
        size_t start = args.rfind(L' ', mktwPos);
        if (start == std::wstring::npos) start = 0;
        else start++;
        std::wstring path = args.substr(start);
        while (!path.empty() && (path.front() == L'"' || path.front() == L' '))
            path.erase(path.begin());
        while (!path.empty() && path.back() == L'"') path.pop_back();
        if (!path.empty()) {
            std::wstring err;
            preset::ImportSettings(path, &err);
        }
    }
}

bool Application::Init(HINSTANCE inst, const wchar_t* cmdLine) {
    g_app = this;
    inst_ = inst;
    embed::Init();
    platform::InstallCrashHandler();
    settings_.Load();
    if (settings_.firRun) {
        settings_.lang = Settings::DetectSystemLanguage();
        settings_.firRun = false;
        settings_.theme = "Dark";
        settings_.Save();
    }
    if (settings_.autoStartExclusive) settings_.exclusiveMode = true;
    settings_.theme = ui::NormalizeTheme(settings_.theme);

    l10n_ = std::make_unique<l10n::Localization>(settings_.lang);

    if (!g_window.Create(inst, brand::kDisplayName)) return false;
    hwnd_ = g_window.Hwnd();
    if (!g_dx.Init(hwnd_)) return false;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ui::InitFonts(hwnd_);
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(g_dx.Device(), g_dx.Context());
    ImGui_ImplDX11_CreateDeviceObjects();
    ui::ApplyTheme(settings_.theme, hwnd_);
    ui::SyncDwmTheme(hwnd_, settings_.theme, g_dx.UsesAlphaSwapChain());
    ui::InitAppShell();

    const bool metricsSvcTray =
        settings_.metricsServiceEnabled && metrics_svc::IsRunning();
    if (!metricsSvcTray) {
        metrics_tray::ShutdownStandalone();
        tray_.Create(hwnd_, inst);
    }

    currentPage_ = PageFromTag(settings_.lastPageTag);
    ParseStartupPage(cmdLine);
    if (currentPage_ == PageId::Count) currentPage_ = PageId::Explorer;

    // Ask before sending anything. ScheduleLaunchEvents is a no-op until the
    // user answers, and the dialog fires the launch events itself on "allow".
    if (analytics::GetConsent() == AnalyticsConsent::Unknown)
        ui::modals::OpenAnalyticsConsent();
    else
        analytics::ScheduleLaunchEvents(settings_.lang);

#ifndef MAKUTWEAKER_BUILD
#define MAKUTWEAKER_BUILD 0
#endif
    if (!settings_.disableUpdateNotify && !settings_.disableTelemetry) {
        platform::CheckForUpdateAsync(MAKUTWEAKER_BUILD, [this](platform::UpdateInfo info) {
            if (info.available) {
                pendingUpdate_ = info;
                showUpdateDialog_ = true;
            }
        });
    }

    if (settings_.metricsServiceEnabled) metrics_svc::EnsureAutostartAndRunning();

    g_window.SetCloseInterceptor([] { return g_app->TryHideToMetricsTray(); });
    g_window.SetMessageHandler([](UINT msg, WPARAM wp, LPARAM lp) {
        if (msg == metrics_tray::kMsgForceQuitMain) {
            DestroyWindow(g_app->hwnd_);
            return;
        }
        if (g_app->Tray().HandleMessage(msg, wp, lp)) return;

        if (ImGui_ImplWin32_WndProcHandler(g_app->hwnd_, msg, wp, lp)) {
            if (msg == WM_DPICHANGED) {
                ui::OnDpiChanged(g_app->hwnd_, g_app->GetSettings().theme,
                                 g_dx.UsesAlphaSwapChain());
            }
            return;
        }

        if (msg == WM_DPICHANGED) {
            const RECT* r = reinterpret_cast<RECT*>(lp);
            SetWindowPos(g_app->hwnd_, nullptr, r->left, r->top, r->right - r->left,
                         r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            ui::OnDpiChanged(g_app->hwnd_, g_app->GetSettings().theme,
                             g_dx.UsesAlphaSwapChain());
            return;
        }

        if (msg == WM_SIZE && wp != SIZE_MINIMIZED) {
            g_dx.Resize(LOWORD(lp), HIWORD(lp));
        }

        if (msg == WM_SETTINGCHANGE) {
            ui::ApplyTheme(g_app->GetSettings().theme, g_app->hwnd_);
        }
    });
    return true;
}

bool Application::TryHideToMetricsTray() {
    if (!settings_.metricsServiceEnabled || !metrics_svc::IsRunning()) return false;
    metrics_tray::EnsureStandaloneTray();
    DestroyWindow(hwnd_);
    return true;
}

void Application::RequestQuit() {
    if (hwnd_) DestroyWindow(hwnd_);
}

void Application::AdoptMetricsServiceTrayMode() {
    if (!settings_.metricsServiceEnabled || !metrics_svc::IsRunning()) return;
    tray_.Destroy();
    metrics_tray::EnsureStandaloneTray();
}

void Application::ReloadLanguage() {
    l10n_ = std::make_unique<l10n::Localization>(settings_.lang);
    l10n_->RebuildSearchIndex();
    ui::ReloadFonts(hwnd_);
    ui::ApplyTheme(settings_.theme, hwnd_);
    ui::SyncDwmTheme(hwnd_, settings_.theme, g_dx.UsesAlphaSwapChain());
}

void Application::NotifyReboot(int mode) {
    auto& l = L10n();
    std::wstring msg;
    if (mode == 1) msg = util::ToWide(l.Def("rebnotify"));
    else if (mode == 2) msg = util::ToWide(l.Def("rebnotifyexplorer"));
    else msg = util::ToWide(l.Def("rebnotifysfc"));
    tray_.ShowBalloon(brand::kDisplayName, msg);
}

void Application::RequestExplorerRestart() {
    static std::atomic<bool> restarting{false};
    if (restarting.exchange(true)) return;
    std::thread([] {
        proc::RestartExplorer();
        restarting = false;
    }).detach();
}

void Application::SetPage(PageId page) {
    if (currentPage_ == PageId::Monitor && page != PageId::Monitor)
        ui::pages::OnMonitorPageClosed();
    currentPage_ = page;
    if (page != PageId::Settings) {
        settings_.lastPageTag = PageTag(page);
        settings_.Save();
        analytics::TrackScreen(settings_.lastPageTag);
    }
}

void Application::Run() {
    // Immediate-mode UI redraws unconditionally, so an unpaced loop burns a core
    // for a window that is not changing. Pace instead:
    //   * live graphs and running jobs need a smooth 60 Hz;
    //   * a focused but quiet window only has to keep up with the caret and
    //     hover effects, so 30 Hz is indistinguishable;
    //   * an unfocused window ticks slowly, and a hidden one does not draw at
    //     all — just enough to keep the tray and message queue responsive.
    constexpr int kActiveFps = 60;
    constexpr int kIdleFps = 30;
    constexpr int kBackgroundFps = 6;

    auto lastInteraction = std::chrono::steady_clock::now();

    while (g_window.ProcessMessages()) {
        const auto frameStart = std::chrono::steady_clock::now();

        // Collect on every frame, not only while the Monitor page is open:
        // otherwise history has a hole for the whole time the app was running
        // on any other page. Tick() throttles itself to the configured
        // interval, so this is a couple of counter reads per second.
        {
            const int intervalSec =
                settings_.metricsIntervalSec > 0 ? settings_.metricsIntervalSec : 2;
            const int uiRefreshMs =
                currentPage_ == PageId::Monitor ? settings_.monitoringRefreshMs : 0;
            metrics::MetricsBackground::Instance().Tick(intervalSec, uiRefreshMs);
        }

        const bool visible = hwnd_ && IsWindowVisible(hwnd_) && !IsIconic(hwnd_);
        const bool focused = visible && GetForegroundWindow() == hwnd_;

        int targetFps = kBackgroundFps;
        if (visible) {
            const ImGuiIO& io = ImGui::GetIO();
            const bool interacting = io.WantCaptureMouse || io.WantCaptureKeyboard ||
                                     ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered() ||
                                     io.MouseDelta.x != 0.f || io.MouseDelta.y != 0.f;
            if (interacting) lastInteraction = frameStart;

            const bool recentlyBusy = frameStart - lastInteraction < std::chrono::seconds(1);
            const bool needsAnimation = currentPage_ == PageId::Monitor ||
                                        jobs::JobQueue::Instance().IsBusy() || recentlyBusy;
            if (needsAnimation)
                targetFps = kActiveFps;
            else if (focused)
                targetFps = kIdleFps;
        }

        if (visible) {
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

        const auto budget = std::chrono::microseconds(1000000 / targetFps);
        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        if (elapsed >= budget) continue;

        // Sleep on the message queue rather than the clock: input wakes us
        // immediately, so a low idle rate never feels laggy.
        const auto remaining =
            std::chrono::duration_cast<std::chrono::milliseconds>(budget - elapsed).count();
        if (remaining > 0)
            MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(remaining), QS_ALLINPUT,
                                        MWMO_INPUTAVAILABLE);
    }
}

void Application::Shutdown() {
    tray_.Destroy();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    g_dx.Shutdown();
}

} // namespace maku::app
