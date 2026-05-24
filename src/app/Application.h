#pragma once
#include "core/JobQueue.h"
#include "core/Localization.h"
#include "core/Settings.h"
#include "platform/TrayIcon.h"
#include "platform/UpdateChecker.h"
#include <memory>
#include <string>

struct ImGuiContext;

namespace maku::app {

enum class PageId {
    Explorer,
    WindowsUpdate,
    SystemRecovery,
    Personalization,
    Uwp,
    QuickSetup,
    Advanced,
    Components,
    Activation,
    Performance,
    ShutdownTimer,
    ProcessMgr,
    Pci,
    Monitor,
    Settings,
    Count
};

inline const char* PageTag(PageId id) {
    static const char* tags[] = {"exp", "wu",   "sys",  "per",  "uwp", "quick", "adv",
                                 "compon", "act", "perf", "sat",  "pmgr", "pci", "mon"};
    return id == PageId::Settings ? "settings" : tags[static_cast<int>(id)];
}

inline PageId PageFromTag(const std::string& tag) {
    if (tag == "settings") return PageId::Settings;
    if (tag == "expl") return PageId::Explorer;
    if (tag == "sr") return PageId::SystemRecovery;
    if (tag == "procmgr") return PageId::ProcessMgr;
    for (int i = 0; i < static_cast<int>(PageId::Settings); ++i) {
        if (tag == PageTag(static_cast<PageId>(i))) return static_cast<PageId>(i);
    }
    return PageId::Explorer;
}

class Application {
public:
    static Application& Instance();

    bool Init(HINSTANCE inst);
    void Run();
    void Shutdown();

    Settings& GetSettings() { return settings_; }
    l10n::Localization& L10n() { return *l10n_; }
    void ReloadLanguage();
    void RequestLanguageReload();
    void RequestFontReload();
    platform::TrayIcon& Tray() { return tray_; }

    void NotifyReboot(int mode); // 1=reboot 2=explorer 3=sfc
    void RequestExplorerRestart();
    void SetPage(PageId page);
    PageId CurrentPage() const { return currentPage_; }

    struct LaunchConfig {
        bool topmost = false;
        bool pciCompact = false;
        bool safeMode = false;
        PageId startPage = PageId::Count;
        std::string pendingLang;
    };
    void ConfigureLaunch(const LaunchConfig& cfg) { launch_ = cfg; }
    const LaunchConfig& Launch() const { return launch_; }

    HWND Hwnd() const { return hwnd_; }
    void SetTopmost(bool on);
    bool TryHideToMetricsTray();
    void RequestQuit();
    /// Service started while GUI is open: hand off tray to metrics helper, keep window alive.
    void AdoptMetricsServiceTrayMode();

    /// Re-apply DWM chrome when theme changes.
    void RefreshGlassPipeline();

    /// Swap-chain mode name for diagnostics (Settings / About).
    const char* RendererGlassInfo() const;
    bool ShowUpdateDialog() const { return showUpdateDialog_; }
    void SetShowUpdateDialog(bool v) { showUpdateDialog_ = v; }
    platform::UpdateInfo PendingUpdate() const { return pendingUpdate_; }
    void SetPendingUpdate(platform::UpdateInfo info) { pendingUpdate_ = info; }

private:
    Application() = default;
    HINSTANCE inst_{};
    HWND hwnd_{};
    Settings settings_;
    std::unique_ptr<l10n::Localization> l10n_;
    platform::TrayIcon tray_;
    PageId currentPage_{PageId::Explorer};
    LaunchConfig launch_{};
    bool explorerRestartPending_{false};
    DWORD explorerRestartAt_{0};
    bool showUpdateDialog_{false};
    platform::UpdateInfo pendingUpdate_{};
    bool pendingFontReload_{false};
    bool pendingLanguageReload_{false};
};

} // namespace maku::app
