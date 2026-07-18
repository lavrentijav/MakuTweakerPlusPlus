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
    WinInfo,
    Monitor,
    Settings,
    Count
};

inline const char* PageTag(PageId id) {
    static const char* tags[] = {"exp",  "wu",   "sys",  "per",  "uwp", "quick", "adv",
                                 "compon", "act", "perf", "sat",  "pmgr", "pci", "wininfo",
                                 "mon"};
    if (id == PageId::Settings) return "settings";
    if (static_cast<int>(id) < 0 || static_cast<int>(id) >= static_cast<int>(PageId::Settings))
        return "exp";
    return tags[static_cast<int>(id)];
}

inline PageId PageFromTag(const std::string& tag) {
    if (tag == "settings") return PageId::Settings;
    if (tag == "expl") return PageId::Explorer;
    if (tag == "sr" || tag == "sys") return PageId::SystemRecovery;
    if (tag == "procmgr" || tag == "mgr") return PageId::ProcessMgr;
    if (tag == "wininfo" || tag == "win") return PageId::WinInfo;
    if (tag == "pci" || tag == "pc") return PageId::Pci;
    if (tag == "mon") return PageId::Monitor;
    if (tag == "wu" || tag == "u") return PageId::WindowsUpdate;
    if (tag == "perf" || tag == "p") return PageId::Performance;
    if (tag == "sat" || tag == "s") return PageId::ShutdownTimer;
    for (int i = 0; i < static_cast<int>(PageId::Settings); ++i) {
        if (tag == PageTag(static_cast<PageId>(i))) return static_cast<PageId>(i);
    }
    return PageId::Explorer;
}

class Application {
public:
    static Application& Instance();

    bool Init(HINSTANCE inst, const wchar_t* cmdLine = nullptr);
    void Run();
    void Shutdown();

    Settings& GetSettings() { return settings_; }
    l10n::Localization& L10n() { return *l10n_; }
    void ReloadLanguage();
    platform::TrayIcon& Tray() { return tray_; }

    void NotifyReboot(int mode); // 1=reboot 2=explorer 3=sfc
    void RequestExplorerRestart();
    void SetPage(PageId page);
    PageId CurrentPage() const { return currentPage_; }
    bool ExclusiveLayout() const { return settings_.exclusiveMode; }

    bool TryHideToMetricsTray();
    void RequestQuit();
    void AdoptMetricsServiceTrayMode();

    HWND Hwnd() const { return hwnd_; }
    bool ShowUpdateDialog() const { return showUpdateDialog_; }
    void SetShowUpdateDialog(bool v) { showUpdateDialog_ = v; }
    platform::UpdateInfo PendingUpdate() const { return pendingUpdate_; }
    void SetPendingUpdate(platform::UpdateInfo info) { pendingUpdate_ = info; }

private:
    Application() = default;
    void ParseStartupPage(const wchar_t* cmdLine);

    HINSTANCE inst_{};
    HWND hwnd_{};
    Settings settings_;
    std::unique_ptr<l10n::Localization> l10n_;
    platform::TrayIcon tray_;
    PageId currentPage_{PageId::Explorer};
    bool showUpdateDialog_{false};
    platform::UpdateInfo pendingUpdate_{};
    DWORD lastPerfTick_{0};
};

} // namespace maku::app
