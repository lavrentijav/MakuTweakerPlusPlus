#include "app/Application.h"
#include "core/OsUtil.h"
#include "core/PerformanceMonitor.h"
#include "core/ProcessRunner.h"
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
#include "platform/D3D11Context.h"

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
    if (args.find(L".mktw") != std::wstring::npos) {
        // preset import handled in settings page if file passed
    }
}

bool Application::Init(HINSTANCE inst, const wchar_t* cmdLine) {
    g_app = this;
    platform::InstallCrashHandler();
    settings_.Load();
    if (settings_.firRun) {
        settings_.lang = Settings::DetectSystemLanguage();
        settings_.firRun = false;
        settings_.theme = "Dark";
        settings_.Save();
    }
    if (settings_.autoStartExclusive) settings_.exclusiveMode = true;

    l10n_ = std::make_unique<l10n::Localization>(settings_.lang);

    if (!g_window.Create(inst, L"MakuTweaker")) return false;
    hwnd_ = g_window.Hwnd();
    if (!g_dx.Init(hwnd_)) return false;

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
    ui::SyncDwmDark(hwnd_, settings_.theme == "Dark");
    ui::InitAppShell();
    tray_.Create(hwnd_, inst);

    currentPage_ = PageFromTag(settings_.lastPageTag);
    ParseStartupPage(cmdLine);
    if (currentPage_ == PageId::Count) currentPage_ = PageId::Explorer;

    perfmon::PerformanceMonitor::Instance().Start();

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

    g_window.SetMessageHandler([](UINT msg, WPARAM wp, LPARAM lp) {
        if (ImGui_ImplWin32_WndProcHandler(g_app->hwnd_, msg, wp, lp)) return;
        if (msg == WM_SIZE && wp != SIZE_MINIMIZED) {
            g_dx.Resize(LOWORD(lp), HIWORD(lp));
        }
    });
    return true;
}

void Application::ReloadLanguage() {
    l10n_ = std::make_unique<l10n::Localization>(settings_.lang);
    ui::ReloadFonts(hwnd_);
    ui::InitAppShell();
}

void Application::NotifyReboot(int mode) {
    auto& l = L10n();
    std::wstring msg;
    if (mode == 1) msg = util::ToWide(l.Def("rebnotify"));
    else if (mode == 2) msg = util::ToWide(l.Def("rebnotifyexplorer"));
    else msg = util::ToWide(l.Def("rebnotifysfc"));
    tray_.ShowBalloon(L"MakuTweaker", msg);
}

void Application::RequestExplorerRestart() {
    explorerRestartPending_ = true;
    explorerRestartAt_ = GetTickCount() + 2000;
    proc::KillExplorer();
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
            proc::Run(L"explorer.exe", L"");
            explorerRestartPending_ = false;
        }

        const DWORD now = GetTickCount();
        if (now - lastPerfTick_ >= static_cast<DWORD>(settings_.monitoringRefreshMs)) {
            perfmon::PerformanceMonitor::Instance().Tick();
            lastPerfTick_ = now;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ui::DrawAppShell();
        ImGui::Render();
        const float clear[] = {0.1f, 0.1f, 0.12f, 1.f};
        g_dx.BeginFrame(clear);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_dx.EndFrame();
    }
}

void Application::Shutdown() {
    perfmon::PerformanceMonitor::Instance().Stop();
    tray_.Destroy();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_dx.Shutdown();
}

} // namespace maku::app
