#include "ui/Extras.h"
#include "ui/UiCommon.h"
#include "app/Application.h"
#include "core/Automation.h"
#include "core/DnsBench.h"
#include "core/JobQueue.h"
#include "core/ProcessWatchdog.h"
#include "core/Rollback.h"
#include "core/ShellMenu.h"
#include "core/HwMonitor.h"
#include "core/WingetUtil.h"
#include "core/OsUtil.h"
#include "core/PendingUi.h"
#include "core/StringUtil.h"
#include <imgui.h>

namespace maku::ui::extras {
namespace {

using app::Application;

static std::string T(const char* key, const char* en) {
    const std::string v = Application::Instance().L10n().Get("tools", "main", key);
    return v == key ? en : v;
}

} // namespace

void DrawAutomationPanel() {
    auto& s = Application::Instance().GetSettings();
    if (ImGui::CollapsingHeader(T("automation", "Automation").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Checkbox(T("ttl_boot", "Apply TTL 65 on every boot (Task Scheduler)").c_str(),
                          &s.autoTtlOnBoot)) {
            const bool want = s.autoTtlOnBoot;
            if (want && !os::IsElevated()) {
                pending::StageBool("autoTtlOnBoot", true);
                pending::Save();
                if (!os::EnsureAdmin(nullptr)) {
                    pending::Remove("autoTtlOnBoot");
                    pending::Save();
                    s.autoTtlOnBoot = false;
                }
            } else {
                if (want)
                    auto_task::InstallBootTtl();
                else {
                    auto_task::RemoveBootTtl();
                    pending::Remove("autoTtlOnBoot");
                    pending::Save();
                }
                s.Save();
            }
        }
        if (ImGui::Checkbox(T("weekly_clean", "Weekly temp/log cleanup (Sunday 03:00)").c_str(),
                          &s.autoWeeklyCleanup)) {
            const bool want = s.autoWeeklyCleanup;
            if (want && !os::IsElevated()) {
                pending::StageBool("autoWeeklyCleanup", true);
                pending::Save();
                if (!os::EnsureAdmin(nullptr)) {
                    pending::Remove("autoWeeklyCleanup");
                    pending::Save();
                    s.autoWeeklyCleanup = false;
                }
            } else {
                if (want)
                    auto_task::InstallWeeklyCleanup();
                else {
                    auto_task::RemoveWeeklyCleanup();
                    pending::Remove("autoWeeklyCleanup");
                    pending::Save();
                }
                s.Save();
            }
        }
        if (ImGui::Checkbox(T("watchdog", "Blocklist watchdog (kill banned apps in background)").c_str(),
                          &s.processWatchdog)) {
            watchdog::SetEnabled(s.processWatchdog);
            s.Save();
        }
        if (ImGui::Button(T("clean_now", "Clean temp now").c_str())) {
            jobs::JobQueue::Instance().Enqueue([] { auto_task::RunTempCleanup(); });
        }
    }
}

void DrawNetworkPanel() {
    auto& s = Application::Instance().GetSettings();
    static std::vector<dns::ServerResult> results;
    static bool gamingTcpInit = false;
    static bool gamingTcp = false;
    if (!gamingTcpInit) {
        gamingTcp = s.gamingTcp;
        gamingTcpInit = true;
    }
    if (ImGui::CollapsingHeader(T("network", "Network booster").c_str(),
                                ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button(T("dns_bench", "Benchmark DNS").c_str())) {
            jobs::JobQueue::Instance().Enqueue([] {
                results = dns::BenchmarkServers();
            });
        }
        for (const auto& r : results) {
            ImGui::Bullet();
            ImGui::SameLine();
            if (r.pingMs >= 0)
                ImGui::Text("%ls (%ls): %d ms", r.name.c_str(), r.address.c_str(), r.pingMs);
            else
                ImGui::Text("%ls (%ls): timeout", r.name.c_str(), r.address.c_str());
            ImGui::SameLine();
            if (ImGui::SmallButton((util::ToUtf8(r.name) + "##dns").c_str())) {
                if (!os::EnsureAdmin(nullptr)) continue;
                if (dns::ApplyDns(r.address)) {
                    s.preferredDns = util::ToUtf8(r.address);
                    s.Save();
                }
            }
        }
        if (ImGui::Checkbox(T("gaming_tcp", "Gaming TCP tweaks (Nagle off)").c_str(), &gamingTcp)) {
            const bool want = gamingTcp;
            if (want && !os::IsElevated()) {
                pending::StageBool("gamingTcp", true);
                pending::Save();
                if (!os::EnsureAdmin(nullptr)) {
                    pending::Remove("gamingTcp");
                    pending::Save();
                    gamingTcp = false;
                }
            } else {
                dns::ApplyGamingTcpTweaks(gamingTcp);
                if (!want) {
                    pending::Remove("gamingTcp");
                    pending::Save();
                }
                s.gamingTcp = gamingTcp;
                s.Save();
            }
        }
    }
}

void DrawWingetPanel() {
    static std::string log;
    if (ImGui::CollapsingHeader(T("winget", "Install software (Winget)").c_str())) {
        if (!winget::IsAvailable()) {
            ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "winget not found in PATH");
            return;
        }
        for (const auto& app : winget::Catalog()) {
            if (ImGui::Button(app.label.c_str())) {
                jobs::JobQueue::Instance().Enqueue([id = app.id]() {
                    std::string logLocal;
                    winget::Install(id, logLocal);
                    log = logLocal;
                });
            }
        }
        if (!log.empty()) ImGui::TextWrapped("%s", log.c_str());
    }
}

void DrawShellPanel() {
    auto& s = Application::Instance().GetSettings();
    if (ImGui::CollapsingHeader(T("shell", "Explorer context menu").c_str())) {
        if (ImGui::Checkbox(T("shell_install", "Add MakuTweaker++ items to right-click menu").c_str(),
                          &s.shellContextMenu)) {
            if (s.shellContextMenu)
                shellmenu::Install();
            else
                shellmenu::Uninstall();
            s.Save();
        }
    }
}

void DrawRollbackPanel() {
    if (ImGui::CollapsingHeader(T("rollback", "Rollback").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextWrapped("%s",
                           T("rollback_info",
                              "Before each preset, registry backup is created. Use restore if something breaks.")
                               .c_str());
        if (rollback::HasSnapshot())
            ImGui::TextColored(ImVec4(0.4f, 1, 0.5f, 1), "%s",
                               T("snapshot_ok", "Snapshot available").c_str());
        else
            ImGui::TextDisabled("%s", T("snapshot_no", "No snapshot yet").c_str());
        if (ImGui::Button(T("restore", "Restore last backup").c_str())) {
            if (os::EnsureAdmin(nullptr)) {
                jobs::JobQueue::Instance().Enqueue([] {
                    rollback::RestoreLatest();
                    Application::Instance().NotifyReboot(1);
                });
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(T("restore_point", "Create system restore point").c_str())) {
            if (os::EnsureAdmin(nullptr)) {
                jobs::JobQueue::Instance().Enqueue([] { rollback::CreateSystemRestorePoint(); });
            }
        }
    }
}

void DrawHwMonitorPanel() {
    static hwmon::Snapshot hw;
    static bool loaded = false;
    if (ImGui::CollapsingHeader(T("hwmon", "Hardware health").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button(T("refresh_hw", "Refresh sensors").c_str()) || !loaded) {
            hw = hwmon::Collect();
            loaded = true;
        }
        for (const auto& t : hw.temps) {
            if (t.celsius >= 0)
                ImGui::Text("%ls: %d C", t.name.c_str(), t.celsius);
        }
        if (hw.temps.empty()) ImGui::TextDisabled("%s", T("no_temp", "No temperature sensors via WMI").c_str());
        for (const auto& d : hw.disks) {
            if (d.healthPercent >= 0)
                ImGui::Text("%ls: %ls (%d%%)", d.name.c_str(), d.status.c_str(), d.healthPercent);
            else
                ImGui::Text("%ls: %ls", d.name.c_str(), d.status.c_str());
        }
    }
}

} // namespace maku::ui::extras
