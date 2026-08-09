#include "ui/Pages.h"
#include "ui/UiCommon.h"
#include "ui/Theme.h"
#include "ui/Modals.h"
#include "app/Application.h"
#include "app/Branding.h"
#include "app/Version.h"
#include "core/AppPaths.h"
#include "ui/MonitorPage.h"
#include "core/PresetIO.h"
#include "core/Benchmark.h"
#include "core/JobQueue.h"
#include "core/ProcessMgrUtil.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include "core/StringUtil.h"
#include "core/Analytics.h"
#include "core/SysInfoFormat.h"
#include "core/UwpUtil.h"
#include "core/TweakRegistry.h"
#include "core/Wmi.h"
#include "ui/TweakPage.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <tlhelp32.h>
#include <psapi.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <mutex>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <fstream>
#include <unordered_map>
#include <map>

namespace maku::ui::pages {
namespace {

using app::Application;

static const char* kLangTags[] = {
    "en", "ru", "uk", "be", "kk", "cs", "de", "fr", "es", "it", "pt", "fi", "et",
    "lv", "pl", "az", "tr", "zh", "tw", "ja", "ko", "vi", "th", "id", "tl", "hi"};

// --- Explorer ---
static void DrawExplorer() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("expl", "main", "label"));
    DrawTweakPage("exp");

    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
    ImGui::TextWrapped("%s", l.Get("expl", "main", "driveslabel").c_str());

    static bool hideDlg = false;
    if (ImGui::Button(l.Get("expl", "main", "choose").c_str())) {
        hideDlg = true;
        ImGui::OpenPopup("HideDrives");
    }

    if (ImGui::BeginPopupModal("HideDrives", &hideDlg, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", l.Get("expl", "status", "hdInfo1").c_str());
        ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));

        // NoDrives is a 26-bit mask, bit 0 = A:. Seed the checkboxes from the
        // live value so reopening the dialog does not silently unhide drives.
        static bool letters[26]{};
        static bool seeded = false;
        if (!seeded) {
            const DWORD mask =
                reg::TryGetDword(HKEY_CURRENT_USER,
                                 L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                                 L"NoDrives")
                    .value_or(0);
            for (int i = 0; i < 26; ++i) letters[i] = (mask & (1u << i)) != 0;
            seeded = true;
        }

        // 26 letters in a 7-column grid keeps the popup roughly square.
        for (int i = 0; i < 26; ++i) {
            char lbl[2] = {static_cast<char>('A' + i), 0};
            ImGui::Checkbox(lbl, &letters[i]);
            if ((i % 7) != 6 && i != 25) ImGui::SameLine();
        }

        ImGui::Dummy(ImVec2(0.f, 8.f * UiScale()));
        if (ImGui::Button(l.Get("expl", "status", "hide").c_str())) {
            DWORD mask = 0;
            for (int i = 0; i < 26; ++i)
                if (letters[i]) mask |= (1u << i);
            reg::SetDword(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                          L"NoDrives", mask);
            Application::Instance().NotifyReboot(2);
            hideDlg = false;
            seeded = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(l.Get("expl", "status", "cc").c_str())) {
            hideDlg = false;
            seeded = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void DrawWindowsUpdate() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("wu", "main", "label"));
    DrawTweakPage("wu");

    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));

    // Feature-update pinning is a choice, not a toggle, so it lives outside the
    // registry-driven rows above.
    ImGui::TextWrapped("%s", l.Get("wu", "main", "wu2").c_str());

    const auto& options = tweak::TargetReleaseOptions();
    static std::string current;
    static float lastRead = -1000.f;
    if (ImGui::GetTime() - lastRead > 3.f) {
        current = tweak::GetTargetRelease();
        lastRead = static_cast<float>(ImGui::GetTime());
    }

    static int selected = -1;
    if (selected < 0) {
        selected = 0;
        for (size_t i = 0; i < options.size(); ++i)
            if (options[i] == current) selected = static_cast<int>(i);
    }

    ImGui::SetNextItemWidth(200.f * UiScale());
    if (ImGui::BeginCombo("##wu_target", options[static_cast<size_t>(selected)].c_str())) {
        for (int i = 0; i < static_cast<int>(options.size()); ++i) {
            const bool isSel = (selected == i);
            if (ImGui::Selectable(options[static_cast<size_t>(i)].c_str(), isSel)) selected = i;
            if (isSel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(l.Get("base", "def", "apply").c_str())) {
        if (os::EnsureAdmin(Application::Instance().Hwnd())) {
            tweak::SetTargetRelease(options[static_cast<size_t>(selected)]);
            lastRead = -1000.f;
            Application::Instance().NotifyReboot(1);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(l.Get("base", "def", "off").c_str())) {
        if (os::EnsureAdmin(Application::Instance().Hwnd())) {
            tweak::ClearTargetRelease();
            lastRead = -1000.f;
        }
    }
    if (current.empty())
        ImGui::TextDisabled("-");
    else
        ImGui::Text("%s", current.c_str());
}

static void DrawSysAndRec() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("sr", "main", "label"));
    DrawTweakPage("sys");
}

static void DrawPersonalization() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("per", "main", "label"));
    DrawTweakPage("per");

    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));

    // Selection highlight color: the original ships eight presets written to
    // Control Panel\Colors as space-separated "R G B" strings.
    struct ColorPreset {
        const char* label;
        const wchar_t* highlight;
        const wchar_t* hotTrack;
        ImVec4 swatch;
    };
    static const ColorPreset kPresets[] = {
        {"c1", L"51 153 255", L"0 102 204", ImVec4(0.20f, 0.60f, 1.00f, 1.f)},
        {"c2", L"0 100 100", L"0 100 100", ImVec4(0.00f, 0.39f, 0.39f, 1.f)},
        {"c3", L"180 0 180", L"110 0 110", ImVec4(0.71f, 0.00f, 0.71f, 1.f)},
        {"c4", L"0 90 30", L"0 90 30", ImVec4(0.00f, 0.35f, 0.12f, 1.f)},
        {"c5", L"100 40 0", L"100 40 0", ImVec4(0.39f, 0.16f, 0.00f, 1.f)},
        {"c6", L"135 0 0", L"135 0 0", ImVec4(0.53f, 0.00f, 0.00f, 1.f)},
        {"c7", L"15 0 120", L"15 0 120", ImVec4(0.06f, 0.00f, 0.47f, 1.f)},
        {"c8", L"40 40 40", L"40 40 40", ImVec4(0.16f, 0.16f, 0.16f, 1.f)},
    };

    ImGui::TextWrapped("%s", l.Get("per", "main", "colorlabel").c_str());
    const float swatch = 28.f * UiScale();
    for (int i = 0; i < static_cast<int>(std::size(kPresets)); ++i) {
        const auto& preset = kPresets[i];
        ImGui::PushID(preset.label);
        ImGui::PushStyleColor(ImGuiCol_Button, preset.swatch);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, preset.swatch);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, preset.swatch);
        if (ImGui::Button("##sw", ImVec2(swatch, swatch))) {
            reg::SetString(HKEY_CURRENT_USER, L"Control Panel\\Colors", L"HightLight",
                           preset.highlight);
            reg::SetString(HKEY_CURRENT_USER, L"Control Panel\\Colors", L"Hilight",
                           preset.highlight);
            reg::SetString(HKEY_CURRENT_USER, L"Control Panel\\Colors", L"HotTrackingColor",
                           preset.hotTrack);
            Application::Instance().NotifyReboot(1);
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", l.Get("per", "main", preset.label).c_str());
        ImGui::PopID();
        if (i != static_cast<int>(std::size(kPresets)) - 1) ImGui::SameLine();
    }
}

static void DrawAdvanced() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.CatName("adv"));
    DrawTweakPage("adv");

    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
    ImGui::Separator();
    ImGui::TextUnformatted(l.Get("adv", "main", "sitebantitle").c_str());
    if (ImGui::Button(l.Get("adv", "main", "sitebanopen").c_str())) modals::OpenSiteBan();
}

static void DrawComponents() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("compon", "main", "label"));
    DrawTweakPage("compon");
}

static void DrawQuickSet() { DrawQuickSetup(); }

/// Confirms the OS still has our delayed shutdown queued.
///
/// Windows offers no read-only query for a pending shutdown, but arming one is
/// a usable probe: `shutdown -s` fails with ERROR_SHUTDOWN_IS_SCHEDULED when a
/// shutdown is already queued, and succeeds when there is none. So a failure
/// means "still armed, nothing changed", and a success means the timer had been
/// cancelled behind our back — we undo the accidental re-arm and report it.
///
/// Runs on the job queue, never on the UI thread, and never within half a
/// minute of the deadline so the probe cannot race the shutdown itself.
bool ShutdownStillScheduled(int remainingSeconds) {
    if (remainingSeconds <= 30) return true;
    const auto armed =
        proc::Run(L"C:\\Windows\\System32\\shutdown.exe",
                  L"-s -t " + std::to_wstring(remainingSeconds));
    if (armed.exitCode != 0) return true;
    proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-a");
    return false;
}

static void DrawSat() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("sat", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("sat", "main", "info").c_str());

    static int mins = 30;
    static std::chrono::steady_clock::time_point shutdownEnds{};
    static std::atomic<bool> shutdownActive{false};
    static std::atomic<bool> verifying{false};
    static double lastVerify = 0.0;

    const auto now = std::chrono::steady_clock::now();
    if (shutdownActive.load() && now >= shutdownEnds) shutdownActive = false;

    const long long remaining =
        shutdownActive.load()
            ? std::max(0LL, std::chrono::duration_cast<std::chrono::seconds>(shutdownEnds - now)
                                .count())
            : 0;

    // The timer can be cancelled from a terminal, Task Scheduler or another
    // tool. Re-check periodically so the countdown never keeps ticking against
    // a shutdown that is no longer scheduled.
    if (shutdownActive.load() && !verifying.load() && ImGui::GetTime() - lastVerify > 10.0) {
        lastVerify = ImGui::GetTime();
        verifying = true;
        const int snapshot = static_cast<int>(remaining);
        jobs::JobQueue::Instance().Enqueue([snapshot] {
            if (!ShutdownStillScheduled(snapshot)) shutdownActive = false;
            verifying = false;
        });
    }

    ImGui::SetNextItemWidth(-200.f * UiScale());
    ImGui::SliderInt(l.Get("sat", "main", "minho").c_str(), &mins, 1, 600, "%d min");

    if (shutdownActive.load()) {
        const long long h = remaining / 3600;
        const long long m = (remaining % 3600) / 60;
        const long long sec = remaining % 60;
        char buf[64]{};
        snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld", h, m, sec);
        std::string countdownLabel = l.Get("sat", "main", "countdown");
        if (countdownLabel == "countdown") countdownLabel = "Until shutdown:";
        ImGui::TextColored(ImVec4(0.45f, 0.88f, 0.55f, 1.f), "%s %s", countdownLabel.c_str(), buf);
    } else {
        std::string idle = l.Get("sat", "main", "countdown_idle");
        if (idle == "countdown_idle") idle = "Shutdown timer is not running.";
        ImGui::TextDisabled("%s", idle.c_str());
    }

    auto startShutdown = [&](int seconds) {
        // Clear any existing schedule first: shutdown.exe refuses to arm a
        // second one, which used to leave the UI counting down a timer that
        // was never actually changed.
        proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-a");
        const auto r = proc::Run(L"C:\\Windows\\System32\\shutdown.exe",
                                 L"-s -t " + std::to_wstring(seconds));
        if (r.exitCode != 0) return;
        shutdownEnds = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        shutdownActive = true;
        lastVerify = ImGui::GetTime();
    };

    ButtonRow("10m", l.Get("sat", "main", "tenM"), [&] { startShutdown(600); });
    ButtonRow("1h", l.Get("sat", "main", "oneH"), [&] { startShutdown(3600); });
    ButtonRow("custom", l.Get("sat", "main", "b1"), [&] { startShutdown(mins * 60); });
    ButtonRow("cancel", l.Get("sat", "main", "b2"), [&] {
        proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-a");
        shutdownActive = false;
    });
}

static void DrawPerf() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("perfor", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("perfor", "main", "info").c_str());
    DrawTweakPage("perf");

    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0.f, 6.f * UiScale()));

    // Maximum processor state. Seeded from powercfg so the slider reflects the
    // machine rather than a hardcoded 50%.
    static int pct = -1;
    static std::string lastError;
    if (pct < 0) {
        const int live = tweak::GetCpuThrottleMax();
        pct = live > 0 ? live : 100;
    }

    ImGui::SetNextItemWidth(-160.f * UiScale());
    ImGui::SliderInt("##throttle", &pct, 1, 100, "%d%%");
    ImGui::SameLine();
    if (ImGui::Button(l.Get("perfor", "main", "applyb").c_str())) {
        if (os::EnsureAdmin(Application::Instance().Hwnd())) {
            lastError.clear();
            if (!tweak::SetCpuThrottleMax(pct, &lastError) && lastError.empty())
                lastError = "powercfg failed";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("100%")) {
        if (os::EnsureAdmin(Application::Instance().Hwnd())) {
            pct = 100;
            lastError.clear();
            tweak::SetCpuThrottleMax(100, &lastError);
        }
    }
    if (!lastError.empty())
        ImGui::TextColored(ImVec4(1.f, 0.45f, 0.35f, 1.f), "%s", lastError.c_str());
}

static void DrawAct() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.CatName("act"));
    // The original's activation page is closed source, so it is deliberately
    // absent here rather than half-implemented. Say so plainly.
    ImGui::TextWrapped(
        "In the GitHub version, for obvious reasons, the source code of the Windows activation "
        "page is not available.\n\n"
        "В GitHub-версии по очевидным причинам исходный код страницы активации Windows "
        "недоступен.");
    ImGui::Dummy(ImVec2(0.f, 10.f * UiScale()));
    if (ImGui::Button("GitHub")) proc::OpenUrl(L"https://github.com/MarkAdderly/MakuTweaker");
}

// --- UWP ---
static std::string UwpStr(const l10n::Localization& l, const char* key, const char* enFallback) {
    const std::string v = l.Get("uwp", "main", key);
    return v == key ? enFallback : v;
}

static std::vector<uwp::PackageInfo> g_uwpPackages;
static std::vector<char> g_uwpSelected;
static std::mutex g_uwpMutex;
static std::atomic<bool> g_uwpListReady{false};
static std::atomic<bool> g_uwpListLoading{false};
static std::string g_uwpLastError;

static void RequestUwpPackageReload() {
    bool expected = false;
    if (!g_uwpListLoading.compare_exchange_strong(expected, true)) return;
    g_uwpListReady = false;
    jobs::JobQueue::Instance().Enqueue([]() {
        auto loaded = uwp::ListInstalledPackages();
        std::lock_guard lock(g_uwpMutex);
        g_uwpPackages = std::move(loaded.packages);
        g_uwpLastError = loaded.error;
        g_uwpSelected.assign(g_uwpPackages.size(), 0);
        g_uwpListReady = true;
        g_uwpListLoading = false;
    });
}

static void DrawUwp() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("uwp", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("uwp", "main", "info1").c_str());
    ImGui::TextWrapped("%s", l.Get("uwp", "main", "info2").c_str());
    ImGui::Separator();

    static char searchBuf[256]{};
    static bool showConfirm{};
    static char confirmBuf[64]{};
    static std::vector<std::wstring> pendingRemove;

    if (!g_uwpListReady && !g_uwpListLoading) RequestUwpPackageReload();

    ImGui::TextUnformatted(UwpStr(l, "allapps", "All installed UWP packages").c_str());
    if (ImGui::Button(UwpStr(l, "refreshlist", "Refresh list").c_str())) RequestUwpPackageReload();
    ImGui::SameLine();
    // The original ships a fixed checkbox list; this port lists every installed
    // package, so the curated set becomes a selection shortcut instead.
    const bool selectCurated =
        ImGui::Button(UwpStr(l, "selectcurated", "Select the usual bloatware").c_str());
    ImGui::SameLine();
    const bool clearSelection = ImGui::Button(l.Get("base", "def", "off").c_str());

    if (g_uwpListLoading.load()) {
        ImGui::TextWrapped("%s", UwpStr(l, "loadinglist", "Loading installed UWP packages...").c_str());
        return;
    }

    std::lock_guard uwpLock(g_uwpMutex);

    if (g_uwpSelected.size() != g_uwpPackages.size())
        g_uwpSelected.assign(g_uwpPackages.size(), 0);
    if (selectCurated) {
        for (size_t i = 0; i < g_uwpPackages.size(); ++i)
            if (uwp::IsCuratedRemovable(g_uwpPackages[i].packageFullName)) g_uwpSelected[i] = 1;
    }
    if (clearSelection) std::fill(g_uwpSelected.begin(), g_uwpSelected.end(), char{0});

    if (g_uwpPackages.empty() && !g_uwpLastError.empty())
        ImGui::TextColored(ImVec4(1.f, 0.45f, 0.35f, 1.f), "%s", g_uwpLastError.c_str());
    else if (g_uwpPackages.empty())
        ImGui::TextDisabled("%s",
                            UwpStr(l, "emptylist", "No UWP packages found or list failed to load.")
                                .c_str());

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##uwp_search",
                             UwpStr(l, "searchpkg", "Search by name or package id...").c_str(),
                             searchBuf, sizeof(searchBuf));
    const std::wstring searchW = util::ToWide(searchBuf);
    const std::wstring searchLower = [&] {
        std::wstring s = searchW;
        for (auto& c : s) c = towlower(c);
        return s;
    }();

    if (ImGui::BeginChild("uwp_list", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 2.5f), true)) {
        if (ImGui::BeginTable("uwp_tbl", 2,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("pkg", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("sel", ImGuiTableColumnFlags_WidthFixed, 36.0f);
            for (size_t i = 0; i < g_uwpPackages.size(); ++i) {
                const auto& pkg = g_uwpPackages[i];
                std::wstring hay = pkg.name + L" " + pkg.packageFullName;
                for (auto& c : hay) c = towlower(c);
                if (!searchLower.empty() && hay.find(searchLower) == std::wstring::npos) continue;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(i));
                if (i >= g_uwpSelected.size()) g_uwpSelected.resize(g_uwpPackages.size(), 0);
                bool checked = g_uwpSelected[i] != 0;
                const std::string label = util::ToUtf8(pkg.name);
                ImGui::TextUnformatted(label.c_str());
                ImGui::TextDisabled("%s", util::ToUtf8(pkg.packageFullName).c_str());
                ImGui::TableNextColumn();
                if (ImGui::Checkbox("##sel", &checked)) g_uwpSelected[i] = checked ? 1 : 0;
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();

    size_t selectedCount = 0;
    for (size_t i = 0; i < g_uwpPackages.size() && i < g_uwpSelected.size(); ++i)
        if (g_uwpSelected[i] != 0) ++selectedCount;

    if (selectedCount == 0)
        ImGui::TextDisabled("%s", UwpStr(l, "selectone", "Select packages to remove.").c_str());
    else
        ImGui::Text("%zu selected", selectedCount);

    if (ImGui::Button(UwpStr(l, "removeselected", "Remove selected").c_str(),
                      ImVec2(-1, 0)) &&
        selectedCount > 0) {
        pendingRemove.clear();
        for (size_t i = 0; i < g_uwpPackages.size() && i < g_uwpSelected.size(); ++i) {
            if (g_uwpSelected[i] != 0) pendingRemove.push_back(g_uwpPackages[i].packageFullName);
        }
        showConfirm = true;
    }

    if (showConfirm) ImGui::OpenPopup("UwpConfirm");
    if (ImGui::BeginPopupModal("UwpConfirm", &showConfirm, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", l.Get("uwp", "main", "suredialogT1").c_str());
        ImGui::Text("%zu %s", pendingRemove.size(), l.Get("uwp", "main", "suredialogT2").c_str());
        ImGui::TextWrapped("%s", l.Get("uwp", "main", "suredialogT3").c_str());
        ImGui::TextWrapped("%s", l.Get("uwp", "main", "suredialogT4").c_str());
        ImGui::InputText("##uwp_confirm", confirmBuf, sizeof(confirmBuf));
        if (ImGui::Button(l.Def("on").c_str())) {
            if (strcmp(confirmBuf, "ILOVEMAKUTWEAKER") == 0 && !pendingRemove.empty()) {
                const auto toRemove = pendingRemove;
                g_uwpListLoading = true;
                g_uwpListReady = false;
                jobs::JobQueue::Instance().Enqueue([toRemove]() {
                    uwp::RemovePackages(toRemove);
                    auto loaded = uwp::ListInstalledPackages();
                    std::lock_guard lock(g_uwpMutex);
                    g_uwpPackages = std::move(loaded.packages);
                    g_uwpLastError = loaded.error;
                    g_uwpSelected.assign(g_uwpPackages.size(), 0);
                    g_uwpListReady = true;
                    g_uwpListLoading = false;
                });
                confirmBuf[0] = 0;
                showConfirm = false;
                pendingRemove.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(l.Def("off").c_str())) {
            confirmBuf[0] = 0;
            showConfirm = false;
            pendingRemove.clear();
        }
        ImGui::EndPopup();
    }
}

// --- Process Manager ---
static std::string PmgrStr(const l10n::Localization& l, const char* key, const char* enFallback) {
    const std::string v = l.Get("pmgr", "main", key);
    return v == key ? enFallback : v;
}

static SIZE_T PmgrMemThresholdMb(int filterIndex) {
    static const int thresholdsMb[] = {0, 50, 100, 300, 500, 1000, 2000};
    if (filterIndex < 0 || filterIndex >= static_cast<int>(std::size(thresholdsMb)))
        return 0;
    return static_cast<SIZE_T>(thresholdsMb[filterIndex]) * 1024u * 1024u;
}

static bool IsSystemProcess(const pmgr::ProcRow& p) {
    if (p.critical) return true;
    std::wstring lower = p.path;
    for (auto& c : lower) c = towlower(c);
    return lower.find(L"\\windows\\") != std::wstring::npos;
}

static void DrawProcessMgr() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    auto& settings = app.GetSettings();
    PageTitleCompact(l.CatName("procmgr"));

    static int tab = 0;
    static std::vector<pmgr::ProcRow> procs;
    static std::vector<pmgr::SvcRow> services;
    static char searchBuf[256]{};
    static DWORD selectedPid = 0;
    static std::wstring selectedSvc;
    static bool needRefresh = true;
    static int lastTab = -1;
    static DWORD lastAutoRefreshTick = 0;
    constexpr DWORD kAutoRefreshMs = 4000;

    const std::wstring searchW = util::ToWide(searchBuf);
    if (tab != lastTab) {
        needRefresh = true;
        lastTab = tab;
    }

    const bool dark = IsDarkTheme(settings.theme);
    PushCardSurface(dark);
    PushCompactToolbarStyle();

    if (ImGui::BeginTable("pmgr_toolbar", 2,
                          ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadInnerX)) {
        ImGui::TableSetupColumn("main", ImGuiTableColumnFlags_WidthStretch, 1.f);
        ImGui::TableSetupColumn("acts", ImGuiTableColumnFlags_WidthFixed, 280.f * UiScale());
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (ImGui::BeginTabBar("pmgr_tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
            if (ImGui::BeginTabItem(PmgrStr(l, "tabproc", "Processes").c_str())) {
                tab = 0;
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(PmgrStr(l, "tabsvc", "Services").c_str())) {
                tab = 1;
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button(l.CatName("mon").c_str())) app.SetPage(app::PageId::Monitor);
        ImGui::SameLine(0.f, 4.f);
        if (ImGui::Button(l.Get("pmgr", "main", "settings").c_str()))
            modals::OpenExclusionSettings();
        ImGui::SameLine(0.f, 4.f);
        if (ImGui::Button("MakuYan")) modals::OpenMakuYan();
        if (!app.ExclusiveLayout()) {
            ImGui::SameLine(0.f, 4.f);
            if (ImGui::Button(l.Get("pmgr", "main", "getfullscr").c_str())) {
                settings.exclusiveMode = true;
                settings.Save();
            }
        }

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::SetNextItemWidth(-1.f);
        ImGui::InputTextWithHint("##pmgr_search",
                                 PmgrStr(l, "filtersearch", "Search by name or PID...").c_str(),
                                 searchBuf, sizeof(searchBuf));
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button(l.Get("pmgr", "main", "showall").c_str())) needRefresh = true;
        if (tab == 0) {
            ImGui::SameLine(0.f, 4.f);
            if (ImGui::Button(l.Get("pmgr", "main", "endprocess").c_str()) && selectedPid != 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, selectedPid);
                if (h) {
                    TerminateProcess(h, 0);
                    CloseHandle(h);
                    selectedPid = 0;
                    needRefresh = true;
                }
            }
        }

        if (tab == 0) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            bool groupChanged = false;
            if (ImGui::Checkbox(PmgrStr(l, "group", "Group processes").c_str(), &settings.group))
                groupChanged = true;
            ImGui::SameLine(0.f, 10.f);
            if (ImGui::Checkbox(l.Get("pmgr", "main", "onlyfrozen").c_str(), &settings.onlyFrozen))
                groupChanged = true;
            ImGui::SameLine(0.f, 10.f);
            if (ImGui::Checkbox(l.Get("pmgr", "main", "compact").c_str(), &settings.compact))
                groupChanged = true;

            const char* memFilters[] = {
                l.Get("pmgr", "main", "showall").c_str(),
                l.Get("pmgr", "main", "from50mb").c_str(),
                l.Get("pmgr", "main", "from100mb").c_str(),
                l.Get("pmgr", "main", "from300mb").c_str(),
                l.Get("pmgr", "main", "from500mb").c_str(),
                l.Get("pmgr", "main", "from1000mb").c_str(),
                l.Get("pmgr", "main", "from2000mb").c_str(),
            };
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-1.f);
            if (ImGui::Combo("##memfilter", &settings.lastProcessFilterIndex, memFilters,
                             static_cast<int>(std::size(memFilters))))
                groupChanged = true;
            if (groupChanged) settings.Save();
        }

        ImGui::EndTable();
    }

    PopCompactToolbarStyle();
    PopCardSurface();
    ImGui::Dummy(ImVec2(0.f, 4.f * UiScale()));

    const bool listEmpty = (tab == 0 && procs.empty()) || (tab == 1 && services.empty());
    const DWORD nowTick = GetTickCount();
    const bool autoDue =
        lastAutoRefreshTick == 0 || (nowTick - lastAutoRefreshTick) >= kAutoRefreshMs;
    if (needRefresh || listEmpty || autoDue) {
        if (tab == 0)
            pmgr::RefreshProcesses(procs, settings.onlyFrozen);
        else
            pmgr::RefreshServices(services);
        if (!needRefresh) lastAutoRefreshTick = nowTick;
        needRefresh = false;
    }

    const std::string yesLbl = l.Def("on");
    const std::string noLbl = l.Def("off");
    const SIZE_T memMin = PmgrMemThresholdMb(settings.lastProcessFilterIndex);

    if (tab == 0) {
        struct FilteredProc {
            size_t index;
        };
        std::vector<FilteredProc> visible;
        visible.reserve(procs.size());
        for (size_t i = 0; i < procs.size(); ++i) {
            const auto& p = procs[i];
            if (settings.onlyFrozen && !p.frozen) continue;
            if (memMin > 0 && p.mem < memMin) continue;
            if (pmgr::IsExcluded(p.name, settings.processExclusions)) continue;
            if (!settings.showSystemProcs && IsSystemProcess(p)) continue;

            std::wstring searchHay = p.name + L" " + std::to_wstring(p.pid);
            if (!p.path.empty()) searchHay += L" " + p.path;
            if (!pmgr::MatchesSearch(searchHay, searchW)) continue;
            visible.push_back({i});
        }

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        const int colCount = settings.compact ? 4 : 5;
        const float tableH = ImGui::GetContentRegionAvail().y - 4.0f;

        SIZE_T maxMem = 1;
        for (const auto& fp : visible) maxMem = std::max(maxMem, procs[fp.index].mem);

        auto drawProcRow = [&](const pmgr::ProcRow& p) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(p.pid));
            const bool selected = selectedPid == p.pid;
            ImGui::TableNextColumn();
            const std::string rowLabel = util::ToUtf8(p.name) + "##" + std::to_string(p.pid);
            if (ImGui::Selectable(rowLabel.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                selectedPid = p.pid;
            if (ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem(l.Get("pmgr", "main", "endprocess").c_str())) {
                    pmgr::TerminateProcessesByName(p.name);
                    needRefresh = true;
                }
                if (ImGui::MenuItem(l.Get("pmgr", "main", "location").c_str())) {
                    if (!p.path.empty()) pmgr::OpenProcessLocation(p.path);
                }
                if (ImGui::MenuItem(l.Get("pmgr", "main", "excl").c_str())) {
                    const std::string exe = util::ToUtf8(p.name);
                    auto& excl = settings.processExclusions;
                    if (excl.find(exe) == std::string::npos) {
                        if (!excl.empty()) excl += ",";
                        excl += exe;
                        settings.Save();
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn();
            ImGui::Text("%lu", p.pid);
            ImGui::TableNextColumn();
            {
                const float frac =
                    std::clamp(static_cast<float>(p.mem) / static_cast<float>(maxMem), 0.f, 1.f);
                const ImVec4 accent = AccentColor();
                ImGui::PushStyleColor(ImGuiCol_PlotHistogram, accent);
                ImGui::PushStyleColor(ImGuiCol_PlotHistogramHovered, AccentLightColor());
                ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 10.f * UiScale()), "");
                ImGui::PopStyleColor(2);
                if (settings.ramMbOnly)
                    ImGui::Text("%.0f MB", p.mem / (1024.0 * 1024.0));
                else
                    ImGui::Text("%.1f MB", p.mem / (1024.0 * 1024.0));
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(p.critical ? yesLbl.c_str() : noLbl.c_str());
            if (!settings.compact) {
                ImGui::TableNextColumn();
                if (!p.path.empty())
                    TextWide(p.path);
                else
                    ImGui::TextDisabled("-");
            }
            ImGui::PopID();
        };

        auto drawProcTableHeader = [&]() {
            if (ImGui::BeginTable("procs_tbl", colCount, tableFlags, ImVec2(0, tableH))) {
                ImGui::TableSetupColumn(l.Get("pmgr", "main", "process").c_str());
                ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn(l.Get("pmgr", "main", "memuse").c_str(),
                                        ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(PmgrStr(l, "critical", "Critical").c_str(),
                                        ImGuiTableColumnFlags_WidthFixed, 90.0f);
                if (!settings.compact)
                    ImGui::TableSetupColumn(l.Get("pmgr", "main", "location").c_str());
                ImGui::TableHeadersRow();
                return true;
            }
            return false;
        };

        if (settings.group) {
            std::map<std::wstring, std::vector<size_t>> groups;
            for (const auto& fp : visible) groups[procs[fp.index].name].push_back(fp.index);

            for (auto& [name, indices] : groups) {
                SIZE_T groupMem = 0;
                for (size_t idx : indices) groupMem += procs[idx].mem;
                const std::string nodeLabel =
                    util::ToUtf8(name) + " (" + std::to_string(indices.size()) + " " +
                    PmgrStr(l, "count", "pcs.") + ", " +
                    std::to_string(static_cast<int>(groupMem / (1024 * 1024))) + " MB)";
                if (ImGui::TreeNode(nodeLabel.c_str())) {
                    if (drawProcTableHeader()) {
                        for (size_t idx : indices) drawProcRow(procs[idx]);
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
            }
        } else {
            if (drawProcTableHeader()) {
                for (const auto& fp : visible) drawProcRow(procs[fp.index]);
                ImGui::EndTable();
            }
        }
    } else {
        struct FilteredSvc {
            size_t index;
        };
        std::vector<FilteredSvc> visible;
        visible.reserve(services.size());
        for (size_t i = 0; i < services.size(); ++i) {
            const auto& s = services[i];
            std::wstring searchHay = s.name + L" " + s.displayName;
            if (!pmgr::MatchesSearch(searchHay, searchW)) continue;
            visible.push_back({i});
        }

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
        const float tableH = ImGui::GetContentRegionAvail().y - 4.0f;

        auto drawSvcRow = [&](const pmgr::SvcRow& s) {
            ImGui::TableNextRow();
            ImGui::PushID(util::ToUtf8(s.name).c_str());
            const bool selected = selectedSvc == s.name;
            ImGui::TableNextColumn();
            const std::string rowLabel = util::ToUtf8(s.name) + "##svc";
            if (ImGui::Selectable(rowLabel.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                selectedSvc = s.name;
            ImGui::TableNextColumn();
            TextWide(s.displayName);
            ImGui::TableNextColumn();
            TextWide(pmgr::ServiceStateText(s.state));
            ImGui::TableNextColumn();
            TextWide(pmgr::ServiceStartTypeText(s.startType));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(s.critical ? yesLbl.c_str() : noLbl.c_str());
            ImGui::PopID();
        };

        auto drawSvcTableHeader = [&]() {
            if (ImGui::BeginTable("svc_tbl", 5, tableFlags, ImVec2(0, tableH))) {
                ImGui::TableSetupColumn(PmgrStr(l, "servicename", "Service").c_str());
                ImGui::TableSetupColumn(PmgrStr(l, "displayname", "Display name").c_str());
                ImGui::TableSetupColumn(PmgrStr(l, "status", "Status").c_str(),
                                        ImGuiTableColumnFlags_WidthFixed, 120.0f);
                ImGui::TableSetupColumn(PmgrStr(l, "starttype", "Startup type").c_str(),
                                        ImGuiTableColumnFlags_WidthFixed, 110.0f);
                ImGui::TableSetupColumn(PmgrStr(l, "critical", "Critical").c_str(),
                                        ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableHeadersRow();
                return true;
            }
            return false;
        };

        if (settings.group) {
            std::map<std::wstring, std::vector<size_t>> groups;
            for (const auto& fs : visible) {
                const auto& s = services[fs.index];
                std::wstring key = s.name;
                const size_t dot = key.find(L'.');
                if (dot != std::wstring::npos) key = key.substr(0, dot);
                groups[key].push_back(fs.index);
            }
            for (auto& [prefix, indices] : groups) {
                const std::string nodeLabel =
                    util::ToUtf8(prefix) + " (" + std::to_string(indices.size()) + " " +
                    PmgrStr(l, "count", "pcs.") + ")";
                if (ImGui::TreeNode(nodeLabel.c_str())) {
                    if (drawSvcTableHeader()) {
                        for (size_t idx : indices) drawSvcRow(services[idx]);
                        ImGui::EndTable();
                    }
                    ImGui::TreePop();
                }
            }
        } else {
            if (drawSvcTableHeader()) {
                for (const auto& fs : visible) drawSvcRow(services[fs.index]);
                ImGui::EndTable();
            }
        }
    }
}

static void DrawSettings() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    auto& s = app.GetSettings();
    PageTitle(l.Get("base", "lowtabs", "set"));

    int langIdx = 0;
    for (int i = 0; i < 26; ++i)
        if (s.lang == kLangTags[i]) langIdx = i;

    ImGui::TextUnformatted(l.Get("ab", "main", "lang").c_str());
    ImGui::SameLine();
    // Explicit width: the default is a fraction of the window, which pushed the
    // combo under the right edge on the settings page.
    ImGui::SetNextItemWidth(140.f * UiScale());
    if (ImGui::BeginCombo("##lang", kLangTags[langIdx])) {
        for (int i = 0; i < 26; ++i) {
            const bool selected = (langIdx == i);
            if (ImGui::Selectable(kLangTags[i], selected)) langIdx = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(l.Get("base", "def", "apply").c_str())) {
        s.lang = kLangTags[langIdx];
        s.Save();
        app.ReloadLanguage();
    }

    ImGui::TextUnformatted(l.Get("ab", "main", "th").c_str());
    bool themeDark = s.theme == "Dark";
    if (ImGui::RadioButton(l.Get("ab", "main", "l").c_str(), !themeDark)) {
        themeDark = false;
        s.theme = "Light";
        s.Save();
        ui::ApplyTheme(s.theme, app.Hwnd());
        ui::SyncDwmTheme(app.Hwnd(), s.theme, false);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(l.Get("ab", "main", "d").c_str(), themeDark)) {
        themeDark = true;
        s.theme = "Dark";
        s.Save();
        ui::ApplyTheme(s.theme, app.Hwnd());
        ui::SyncDwmTheme(app.Hwnd(), s.theme, false);
    }
    ImGui::Separator();
    ImGui::Text("%s %s (C++/ImGui)", brand::kDisplayNameUtf8, maku::version::kText);
    ImGui::Text("Mark Adderly");
    if (ImGui::Button("GitHub"))
        proc::OpenUrl(L"https://github.com/MarkAdderly/MakuTweaker");
    if (ImGui::Button("Donate")) proc::OpenUrl(L"https://adderly.top/mt");

    bool winr = apppaths::WinRAliasesEnabled();
    if (ImGui::Checkbox(l.Get("ab", "main", "winr").c_str(), &winr)) apppaths::SetWinRAliases(winr);
    ImGui::TextWrapped("%s", l.Get("ab", "main", "winrinfo").c_str());

    ImGui::Separator();
    ImGui::TextUnformatted(l.Get("ab", "consent", "title").c_str());
    // Opt-*out* polarity here, matching the long explanation below ("just tick
    // the box and collection stops") that is already translated into all 26
    // languages. The first-run dialog asks the opposite way round, which is
    // fine — it is phrased as its own question.
    bool disableAnalytics = s.analyticsConsent != static_cast<int>(AnalyticsConsent::Granted);
    if (ImGui::Checkbox(l.Get("ab", "main", "disabletelemetry").c_str(), &disableAnalytics)) {
        const auto consent =
            disableAnalytics ? AnalyticsConsent::Declined : AnalyticsConsent::Granted;
        analytics::SetConsent(consent);
        s.analyticsConsent = static_cast<int>(consent);
        s.disableTelemetry = disableAnalytics;
    }
    if (ImGui::CollapsingHeader(l.Get("ab", "consent", "details").c_str())) {
        ImGui::TextWrapped("%s", l.Get("ab", "main", "telemetryabt").c_str());
        ImGui::Spacing();
        // Show the numbers that would be shared, so "most visited tabs" is a
        // concrete list rather than a promise.
        ImGui::TextUnformatted(l.Get("ab", "consent", "item_tabs").c_str());
        const auto top = analytics::TopVisitedTabs();
        if (top.empty()) {
            ImGui::TextDisabled("-");
        } else {
            for (const auto& [tag, count] : top)
                ImGui::BulletText("%s - %d", analytics::ScreenName(tag).c_str(), count);
        }
    }

    ImGui::Checkbox(l.Get("pmgr", "main", "modeset").c_str(), &s.autoStartExclusive);

    ImGui::Separator();
    ImGui::TextUnformatted(l.Get("ab", "main", "cfg_title").c_str());
    if (ImGui::Button(l.Get("ab", "main", "cfg_save").c_str())) {
        std::wstring path = util::GetExeDirectory() + L"\\preset.mktw";
        std::wstring err;
        if (preset::ExportSettings(path, &err))
            MessageBoxW(app.Hwnd(), util::ToWide(l.Get("ab", "main", "cfg_svsuccess")).c_str(),
                        brand::kDisplayName, MB_OK);
        else
            MessageBoxW(app.Hwnd(), err.c_str(), brand::kDisplayName, MB_OK | MB_ICONERROR);
    }
    ImGui::SameLine();
    if (ImGui::Button(l.Get("ab", "main", "cfg_import").c_str())) {
        std::wstring path = util::GetExeDirectory() + L"\\preset.mktw";
        std::wstring err;
        if (preset::ImportSettings(path, &err))
            MessageBoxW(app.Hwnd(), util::ToWide(l.Get("ab", "main", "cfg_ldsuccess")).c_str(),
                        brand::kDisplayName, MB_OK);
        else
            MessageBoxW(app.Hwnd(), err.c_str(), brand::kDisplayName, MB_OK | MB_ICONERROR);
    }

    if (ImGui::Button("MakuYan")) modals::OpenMakuYan();
    if (ImGui::Button(l.Get("pmgr", "main", "settings").c_str())) modals::OpenExclusionSettings();
}

} // namespace

void Draw(app::PageId page) {
    if (jobs::JobQueue::Instance().IsBusy()) {
        ImGui::Text("Working: %ls", jobs::JobQueue::Instance().ProgressText().c_str());
        ImGui::ProgressBar(0.5f, ImVec2(-1, 0));
    }
    switch (page) {
    case app::PageId::Explorer: DrawExplorer(); break;
    case app::PageId::WindowsUpdate: DrawWindowsUpdate(); break;
    case app::PageId::SystemRecovery: DrawSysAndRec(); break;
    case app::PageId::Personalization: DrawPersonalization(); break;
    case app::PageId::Uwp: DrawUwp(); break;
    case app::PageId::QuickSetup: DrawQuickSet(); break;
    case app::PageId::Advanced: DrawAdvanced(); break;
    case app::PageId::Components: DrawComponents(); break;
    case app::PageId::Activation: DrawAct(); break;
    case app::PageId::Performance: DrawPerf(); break;
    case app::PageId::ShutdownTimer: DrawSat(); break;
    case app::PageId::ProcessMgr: DrawProcessMgr(); break;
    case app::PageId::Pci: DrawPci(); break;
    case app::PageId::WinInfo: DrawWinInfo(); break;
    case app::PageId::Monitor: DrawMonitor(); break;
    case app::PageId::Settings: DrawSettings(); break;
    default: break;
    }
}

} // namespace maku::ui::pages
