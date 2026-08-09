#include "ui/TweakPage.h"

#include "app/Application.h"
#include "core/OsUtil.h"
#include "core/TweakRegistry.h"
#include "ui/Theme.h"
#include "ui/UiCommon.h"

#include <imgui.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace maku::ui {
namespace {

using app::Application;

constexpr double kProbeIntervalSeconds = 2.0;

/// Probe results for one page. Shared with a detached worker, hence shared_ptr
/// + mutex: the page can be navigated away from while a probe is in flight.
struct PageProbe {
    std::mutex mutex;
    std::vector<char> states;   ///< Parallel to ForPage(); 1 = tweak is on.
    std::atomic<bool> running{false};
    std::atomic<bool> ready{false};
    double lastProbe = -1000.0;
};

std::unordered_map<std::string, std::shared_ptr<PageProbe>> g_probes;

std::shared_ptr<PageProbe> ProbeFor(const std::string& page) {
    auto it = g_probes.find(page);
    if (it != g_probes.end()) return it->second;
    return g_probes.emplace(page, std::make_shared<PageProbe>()).first->second;
}

void StartProbe(const std::string& page, const std::shared_ptr<PageProbe>& probe) {
    bool expected = false;
    if (!probe->running.compare_exchange_strong(expected, true)) return;
    probe->lastProbe = ImGui::GetTime();

    std::thread([page, probe] {
        const auto tweaks = tweak::ForPage(page);
        std::vector<char> states(tweaks.size(), 0);
        for (size_t i = 0; i < tweaks.size(); ++i) {
            if (tweaks[i]->kind != tweak::Kind::Toggle) continue;
            states[i] = tweak::Probe(*tweaks[i]) ? 1 : 0;
        }
        {
            std::lock_guard lock(probe->mutex);
            probe->states = std::move(states);
        }
        probe->ready = true;
        probe->running = false;
    }).detach();
}

void NotifyReboot(tweak::Reboot reboot) {
    if (reboot == tweak::Reboot::None) return;
    Application::Instance().NotifyReboot(static_cast<int>(reboot));
}

/// True when the tweak may run. Admin-only tweaks prompt once for elevation.
bool EnsureRunnable(const tweak::Tweak& t) {
    if (!t.needsAdmin) return true;
    return os::EnsureAdmin(Application::Instance().Hwnd());
}

} // namespace

void InvalidateTweakPage(const char* page) {
    auto it = g_probes.find(page);
    if (it != g_probes.end()) it->second->lastProbe = -1000.0;
}

void DrawTweakPage(const char* page) {
    auto& app = Application::Instance();
    auto& l = app.L10n();

    const std::string pageKey = page;
    const auto tweaks = tweak::ForPage(pageKey);
    const auto probe = ProbeFor(pageKey);

    if (!probe->running.load() && ImGui::GetTime() - probe->lastProbe > kProbeIntervalSeconds)
        StartProbe(pageKey, probe);

    std::vector<char> states;
    {
        std::lock_guard lock(probe->mutex);
        states = probe->states;
    }
    states.resize(tweaks.size(), 0);

    // Toggles first, then one-shot actions — matches the original's layout.
    bool anyToggle = false;
    for (size_t i = 0; i < tweaks.size(); ++i) {
        const tweak::Tweak& t = *tweaks[i];
        if (t.kind != tweak::Kind::Toggle) continue;
        anyToggle = true;

        bool value = states[i] != 0;
        if (ToggleRow(t.id, tweak::DisplayName(t, l), &value)) {
            // ToggleRow reports only real transitions, so this never re-applies.
        }
        if (value != (states[i] != 0)) {
            if (EnsureRunnable(t)) {
                tweak::Apply(t, value);
                NotifyReboot(t.reboot);
            }
            {
                std::lock_guard lock(probe->mutex);
                if (i < probe->states.size()) probe->states[i] = value ? 1 : 0;
            }
            probe->lastProbe = -1000.0; // Re-probe next frame to show the truth.
        }
    }

    bool firstAction = true;
    for (const tweak::Tweak* tp : tweaks) {
        const tweak::Tweak& t = *tp;
        if (t.kind != tweak::Kind::Action) continue;
        if (firstAction) {
            // Pages that are all actions (Windows Components) need no divider.
            if (anyToggle) {
                ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
                ImGui::Separator();
                ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
            }
            firstAction = false;
        }
        if (ButtonRow(t.id, tweak::DisplayName(t, l))) {
            if (EnsureRunnable(t)) {
                const tweak::Tweak* captured = tp;
                jobs::JobQueue::Instance().Enqueue([captured] {
                    tweak::Apply(*captured, true);
                });
                NotifyReboot(t.reboot);
            }
        }
    }
}

const std::vector<std::string>& QuickSetupIds() {
    // Order mirrors the original Quick Setup page: the Explorer/taskbar block
    // first, then the security-relaxing block the original puts behind a second
    // "select all".
    static const std::vector<std::string> ids = {
        "explorer.show-hidden",
        "explorer.show-extensions",
        "explorer.open-this-pc",
        "explorer.show-this-pc-desktop",
        "explorer.clean-shortcut-name",
        "explorer.hide-gallery",
        "explorer.fix-delegate-folders",
        "per.hide-taskbar-widgets",
        "per.no-search-highlights",
        "per.clipboard-history",
        "per.fast-context-menu",
        "per.old-context-menu",
        "per.end-task",
        "per.verbose-boot",
        "sys.bing-search-off",
        "sys.sticky-keys-off",
        "sys.chkdsk-timeout",
        "sys.block-bitlocker",
        "compon.directplay",
        "wu.block-updates",
        "sys.core-isolation-off",
        "sys.uac-off",
        "sys.smartscreen-off",
        "sys.hibernate-off",
        "sys.telemetry-off",
        "adv.vbs-off",
    };
    return ids;
}

void DrawQuickSetup() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    const bool dark = IsDarkTheme(app.GetSettings().theme);

    PageTitle(l.Get("quick", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("quick", "main", "info").c_str());
    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));

    const auto& ids = QuickSetupIds();

    // Selection is per-session; the original also starts with nothing ticked.
    static std::vector<char> selected;
    selected.resize(ids.size(), 0);

    if (ImGui::Button(l.Get("quick", "main", "checkall").c_str()))
        std::fill(selected.begin(), selected.end(), char{1});
    ImGui::SameLine();
    if (ImGui::Button(l.Get("base", "def", "off").c_str()))
        std::fill(selected.begin(), selected.end(), char{0});

    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));

    if (BeginSettingsCard("quick_list", dark, ImGui::GetContentRegionAvail().y - 60.f * UiScale())) {
        for (size_t i = 0; i < ids.size(); ++i) {
            const tweak::Tweak* t = tweak::Find(ids[i]);
            if (!t || !tweak::IsAvailable(*t)) continue;
            ImGui::PushID(static_cast<int>(i));
            bool on = selected[i] != 0;
            if (ImGui::Checkbox(tweak::DisplayName(*t, l).c_str(), &on)) selected[i] = on ? 1 : 0;
            if (t->needsAdmin) {
                ImGui::SameLine();
                ImGui::TextDisabled("(admin)");
            }
            ImGui::PopID();
        }
    }
    EndSettingsCard();

    size_t count = 0;
    for (char c : selected)
        if (c) ++count;

    ImGui::BeginDisabled(count == 0);
    if (ImGui::Button(l.Get("quick", "main", "b").c_str(), ImVec2(-1, 0))) {
        std::vector<std::string> toApply;
        for (size_t i = 0; i < ids.size(); ++i)
            if (selected[i]) toApply.push_back(ids[i]);

        if (os::EnsureAdmin(app.Hwnd())) {
            jobs::JobQueue::Instance().Enqueue([toApply] {
                for (const auto& id : toApply) {
                    const tweak::Tweak* t = tweak::Find(id);
                    if (t) tweak::Apply(*t, true);
                }
            });
            app.NotifyReboot(1);
        }
    }
    ImGui::EndDisabled();
    if (count > 0) ImGui::Text("%zu", count);
}

} // namespace maku::ui
