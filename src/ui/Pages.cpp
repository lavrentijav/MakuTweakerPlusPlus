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
#include "core/TweakProbe.h"
#include "core/Wmi.h"
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

static bool g_loaded = false;

static std::string FormatScore(double score) {
    long long v = static_cast<long long>(std::llround(score));
    std::string s = std::to_string(v);
    for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
        s.insert(static_cast<size_t>(i), " ");
    return s;
}

static bool KeyMissing(HKEY root, const wchar_t* sub) {
    return !reg::KeyExists(root, sub);
}

static void DeleteNamespaceKeys(bool on) {
    const wchar_t* keys[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{A0953C92-50DC-43bf-BE83-3742FED03C9C}",
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{f86fa3ab-70d2-4fc7-9c99-fcbf05467f3a}",
        L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{A0953C92-50DC-43bf-BE83-3742FED03C9C}",
        L"SOFTWARE\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{f86fa3ab-70d2-4fc7-9c99-fcbf05467f3a}",
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{374DE290-123F-4565-9164-39C4925E467B}",
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{B4BFCC3A-DB2C-424C-B029-7FE99A87C641}"};
    for (auto k : keys) {
        if (on) reg::DeleteKeyTree(HKEY_LOCAL_MACHINE, k);
        else reg::CreateKey(HKEY_LOCAL_MACHINE, k);
    }
}

// --- Explorer ---
static void DrawExplorer() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("expl", "main", "label"));
    static bool nonrem, hidden, ext, pchome, gallery, showpc, shortcut;
    static float lastProbe = -1000.f;
    if (ImGui::GetTime() - lastProbe > 2.f) {
        const auto t = tweak::ProbeExplorer();
        nonrem = t.nonremovable;
        hidden = t.hidden;
        ext = t.extensions;
        pchome = t.pcHome;
        gallery = t.gallery;
        showpc = t.showPc;
        shortcut = t.shortcut;
        lastProbe = ImGui::GetTime();
    }
    if (os::GetWindowsBuild() < 22621)
        ToggleRow("nonrem", l.Get("expl", "main", "nonremovable"), &nonrem,
                  [](bool on) { DeleteNamespaceKeys(on); });
    ToggleRow("hidden", l.Get("expl", "main", "hidden"), &hidden, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Hidden",
                      on ? 1u : 0u);
    });
    ToggleRow("ext", l.Get("expl", "main", "ext"), &ext, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                      L"HideFileExt", on ? 0u : 1u);
    });
    ToggleRow("pchome", l.Get("expl", "main", "pchome"), &pchome, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"LaunchTo",
                      on ? 1u : 2u);
    });
    ToggleRow("gallery", l.Get("expl", "main", "gallery"), &gallery, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Classes\\CLSID\\{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}",
                      L"System.IsPinnedToNameSpaceTree", on ? 0u : 1u);
    });
    ToggleRow("showpc", l.Get("expl", "main", "showpc"), &showpc, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel",
                      L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", on ? 0u : 1u);
    });
    ToggleRow("shortcut", l.Get("expl", "main", "shortcut"), &shortcut, [](bool on) {
        if (on)
            reg::SetString(HKEY_CURRENT_USER,
                           L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\NamingTemplates",
                           L"ShortcutNameTemplate", L"%s.lnk");
        else
            reg::DeleteKeyTree(HKEY_CURRENT_USER,
                              L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\NamingTemplates");
    });
    if (ButtonRow("fix", l.Get("expl", "main", "e8b"), [] {
            reg::DeleteKeyTree(HKEY_LOCAL_MACHINE,
                               L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\DelegateFolders\\{F5FB2C77-0E2F-4A16-A381-3E560C68BC83}");
            reg::DeleteKeyTree(HKEY_LOCAL_MACHINE,
                               L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\DelegateFolders\\{F5FB2C77-0E2F-4A16-A381-3E560C68BC83}");
        }))
        Application::Instance().NotifyReboot(2);

    static bool hideDlg{};
    if (ButtonRow("hide", l.Get("expl", "main", "choose"))) hideDlg = true;
    if (hideDlg) {
        ImGui::OpenPopup("HideDrives");
        if (ImGui::BeginPopupModal("HideDrives", &hideDlg, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", l.Get("expl", "status", "hdInfo1").c_str());
            static bool letters[26]{};
            for (int i = 0; i < 26; ++i) {
                char lbl[2] = {static_cast<char>('A' + i), 0};
                ImGui::Checkbox(lbl, &letters[i]);
            }
            if (ImGui::Button(l.Get("expl", "status", "hide").c_str())) {
                DWORD mask = 0;
                for (int i = 0; i < 26; ++i)
                    if (letters[i]) mask |= (1u << i);
                reg::SetDword(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                              L"NoDrives", mask);
                Application::Instance().NotifyReboot(2);
                hideDlg = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(l.Get("expl", "status", "cc").c_str())) hideDlg = false;
            ImGui::EndPopup();
        }
    }
    if (ButtonRow("showall", l.Get("expl", "main", "showall"), [] {
            reg::SetDword(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                          L"NoDrives", 0);
            Application::Instance().NotifyReboot(2);
        })) {
    }
}

static void DrawWindowsUpdate() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("wu", "main", "label"));
    static bool wu1, wu2, wu3, wu5, wu6;
    static float lastProbe = -1000.f;
    if (ImGui::GetTime() - lastProbe > 2.f) {
        const auto t = tweak::ProbeWindowsUpdate();
        wu1 = t.blockInternet;
        wu3 = t.excludeDrivers;
        wu5 = t.serviceStopped;
        wu6 = t.disableReserves;
        lastProbe = ImGui::GetTime();
    }
    ToggleRow("wu1", l.Get("wu", "main", "wu1"), &wu1, [](bool on) {
        if (on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                          L"DoNotConnectToWindowsUpdateInternetLocations", 1);
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                          L"DisableWindowsUpdateAccess", 1);
            jobs::JobQueue::Instance().Enqueue([] {
                proc::Run(L"net", L"stop wuauserv");
                proc::Run(L"sc", L"config wuauserv start= disabled");
            });
        }
    });
    ToggleRow("wu3", l.Get("wu", "main", "wu3"), &wu3, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                      L"ExcludeWUDriversInQualityUpdate", on ? 1u : 0u);
    });
    if (ButtonRow("wu4", l.Get("wu", "main", "wu4"), [] {
            jobs::JobQueue::Instance().Enqueue([] {
                proc::Run(L"net", L"stop wuauserv");
                proc::RunHidden("net stop bits && net stop cryptsvc");
                proc::RunHidden(
                    "ren C:\\Windows\\SoftwareDistribution SoftwareDistribution.old");
                proc::Run(L"net", L"start wuauserv");
            });
        }))
        Application::Instance().NotifyReboot(1);
    ToggleRow("wu5", l.Get("wu", "main", "wu5"), &wu5, [](bool on) {
        proc::Run(L"net", on ? L"stop wuauserv" : L"start wuauserv");
    });
    ToggleRow("wu6", l.Get("wu", "main", "wu6"), &wu6, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ReserveManager",
                      L"ShippedWithReserves", on ? 0u : 1u);
    });
}

static void DrawSysAndRec() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("sr", "main", "label"));
    static bool telemetry, uac, hybern, smartscreen, bing, sticky, bitlocker, coreisol, chkdsk;
    static float lastProbe = -1000.f;
    if (ImGui::GetTime() - lastProbe > 2.f) {
        const auto t = tweak::ProbeSysRec();
        telemetry = t.telemetryOff;
        uac = t.uacOff;
        hybern = t.hibernateOff;
        smartscreen = t.smartScreenOff;
        bing = t.bingOff;
        lastProbe = ImGui::GetTime();
    }
    ToggleRow("telemetry", l.Get("sr", "main", "telemetry"), &telemetry, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry",
                      on ? 0u : 3u);
    });
    ToggleRow("uac", l.Get("sr", "main", "uac"), &uac, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA",
                      on ? 0u : 1u);
    });
    ToggleRow("hybern", l.Get("sr", "main", "hybern"), &hybern,
              [](bool on) { proc::RunHidden(on ? "powercfg /hibernate off" : "powercfg /hibernate on"); });
    ToggleRow("smartscreen", l.Get("sr", "main", "smartscreen"), &smartscreen, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                      L"EnableSmartScreen", on ? 0u : 1u);
    });
    ToggleRow("bing", l.Get("sr", "main", "bing"), &bing, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Windows\\Explorer",
                      L"DisableSearchBoxSuggestions", on ? 1u : 0u);
    });
    if (ButtonRow("sfc", l.Get("sr", "main", "sfclabel"), [] {
            jobs::JobQueue::Instance().Enqueue(
                [] { proc::Run(L"sfc", L"/scannow"); Application::Instance().NotifyReboot(3); });
        })) {
    }
    if (ButtonRow("dism", l.Get("sr", "main", "dismlabel"),
                  [] { jobs::JobQueue::Instance().Enqueue([] { proc::Run(L"dism", L"/Online /Cleanup-Image /RestoreHealth"); }); })) {
    }
    if (ButtonRow("temp", l.Get("sr", "main", "templabel"), [] {
            proc::RunHidden("del /q /f /s %TEMP%\\*");
        })) {
    }
    if (ButtonRow("battery", l.Get("sr", "main", "batterylabel"),
                  [] { proc::RunHidden("powercfg /batteryreport"); })) {
    }
    if (ButtonRow("pipcache", l.Get("sr", "main", "pipcache"), [] {
            jobs::JobQueue::Instance().Enqueue([] {
                proc::RunHidden("powershell -NoProfile -Command \"Remove-Item -Recurse -Force "
                                "$env:LOCALAPPDATA\\pip\\cache -ErrorAction SilentlyContinue\"");
            });
        })) {
    }
}

static void DrawPersonalization() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("per", "main", "label"));
    static bool dark, transparency, verbose, oldcont, endtask;
    static float lastProbe = -1000.f;
    if (ImGui::GetTime() - lastProbe > 2.f) {
        const auto t = tweak::ProbePersonalization();
        dark = t.darkTheme;
        transparency = t.transparencyOff;
        verbose = t.verboseBoot;
        endtask = t.endTask;
        oldcont = t.oldContextMenu;
        lastProbe = ImGui::GetTime();
    }
    ToggleRow("dark", l.Get("per", "main", "darktheme"), &dark, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      L"AppsUseLightTheme", on ? 0u : 1u);
        reg::SetDword(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      L"SystemUsesLightTheme", on ? 0u : 1u);
    });
    ToggleRow("transparency", l.Get("per", "main", "transparency"), &transparency, [](bool on) {
        reg::SetDword(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                      L"EnableTransparency", on ? 0u : 1u);
    });
    ToggleRow("verbose", l.Get("per", "main", "verbose"), &verbose, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"verbosestatus",
                      on ? 1u : 0u);
    });
    if (os::GetWindowsBuild() >= 22000)
        ToggleRow("endtask", l.Get("per", "main", "etask"), &endtask, [](bool on) {
            reg::SetDword(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\TaskbarDeveloperSettings",
                          L"TaskbarEndTask", on ? 1u : 0u);
        });
    if (os::GetWindowsBuild() >= 22621)
        ToggleRow("oldcont", l.Get("per", "main", "oldcont"), &oldcont, [](bool on) {
            if (on)
                reg::CreateKey(HKEY_CURRENT_USER,
                               L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32");
            else
                reg::DeleteKeyTree(HKEY_CURRENT_USER,
                                   L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}");
        });
}

static void DrawAdvanced() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.CatName("adv"));
    static bool vbs, ttl, disindex, swap;
    static float lastProbe = -1000.f;
    if (ImGui::GetTime() - lastProbe > 2.f) {
        const auto t = tweak::ProbeAdvanced();
        vbs = t.vbsOff;
        ttl = t.ttlReduced;
        disindex = t.indexingOff;
        lastProbe = ImGui::GetTime();
    }
    ToggleRow("vbs", l.Get("adv", "main", "vbs"), &vbs, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                      L"EnableVirtualizationBasedSecurity", on ? 0u : 1u);
    });
    ToggleRow("ttl", l.Get("adv", "main", "ttl"), &ttl, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                      L"DefaultTTL", on ? 65u : 128u);
    });
    ToggleRow("disindex", l.Get("adv", "main", "index_title"), &disindex, [](bool on) {
        reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\WSearch", L"Start",
                      on ? 4u : 2u);
    });
    if (ButtonRow("edge", l.Get("adv", "main", "deledge_btn"), [] {
            jobs::JobQueue::Instance().Enqueue([] {
                proc::RunPowerShell(
                    L"Get-AppxPackage *Microsoft.MicrosoftEdge* | Remove-AppxPackage -ErrorAction SilentlyContinue");
            });
        })) {
    }
    ImGui::Separator();
    ImGui::TextUnformatted(l.Get("adv", "main", "sitebantitle").c_str());
    if (ImGui::Button(l.Get("adv", "main", "sitebanopen").c_str())) modals::OpenSiteBan();
}

static void DrawComponents() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("compon", "main", "label"));
    if (ButtonRow("dp", l.Get("compon", "main", "directplay"),
                  [] { proc::Run(L"dism", L"/online /enable-feature /featurename:DirectPlay /all"); }))
    {
    }
    if (ButtonRow("netfx", l.Get("compon", "main", "framework"),
                  [] { proc::Run(L"dism", L"/online /enable-feature /featurename:NetFx3 /all"); }))
    {
    }
    if (ButtonRow("pv", l.Get("compon", "main", "photoviewer"), [] {
            proc::RunHidden(
                "reg add \"HKLM\\SOFTWARE\\Microsoft\\Windows Photo Viewer\\Capabilities\\FileAssociations\" /v .jpg /t REG_SZ /d PhotoViewer.FileAssoc.Tiff /f");
        }))
    {
    }
    if (ButtonRow("gpedit", l.Get("compon", "main", "gpedit"), [] {
            proc::RunPowerShell(
                L"foreach ($i in 'gpedit','fde','gaudit','ppe') { dism /online /add-capability /CapabilityName:Rsat.$i.Tools~~~~0.0.1.0 }");
        }))
    {
    }
    if (ButtonRow("hyperv", l.Get("compon", "main", "forcedis"),
                  [] { proc::Run(L"bcdedit", L"/set hypervisorlaunchtype off"); }))
    {
    }
    if (ButtonRow("winsxs", l.Get("compon", "main", "winsxs"),
                  [] { proc::Run(L"dism", L"/online /Cleanup-Image /StartComponentCleanup /ResetBase"); }))
    {
    }
}

static void DrawQuickSet() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("quick", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("quick", "main", "info").c_str());
    if (ButtonRow("q1", l.Get("quick", "main", "b"), [] {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
                          L"AllowTelemetry", 0);
            proc::RunHidden("powercfg /hibernate off");
        })) {
    }
}

static void DrawSat() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("sat", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("sat", "main", "info").c_str());

    static int mins = 30;
    static std::chrono::steady_clock::time_point shutdownEnds{};
    static bool shutdownActive = false;

    const auto now = std::chrono::steady_clock::now();
    if (shutdownActive && now >= shutdownEnds) shutdownActive = false;

    ImGui::SliderInt(l.Get("sat", "main", "minho").c_str(), &mins, 1, 600);

    if (shutdownActive) {
        const auto left = std::chrono::duration_cast<std::chrono::seconds>(shutdownEnds - now);
        const long long sec = std::max(0LL, left.count());
        const long long h = sec / 3600;
        const long long m = (sec % 3600) / 60;
        const long long s = sec % 60;
        char buf[64]{};
        snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld",
                 static_cast<long long>(h), static_cast<long long>(m), static_cast<long long>(s));
        std::string countdownLabel = l.Get("sat", "main", "countdown");
        if (countdownLabel == "countdown") countdownLabel = "Until shutdown:";
        ImGui::TextColored(ImVec4(0.45f, 0.88f, 0.55f, 1.f), "%s %s", countdownLabel.c_str(), buf);
    } else {
        std::string idle = l.Get("sat", "main", "countdown_idle");
        if (idle == "countdown_idle") idle = "Shutdown timer is not running.";
        ImGui::TextDisabled("%s", idle.c_str());
    }

    auto startShutdown = [&](int seconds) {
        wchar_t args[64];
        swprintf_s(args, L"-s -t %d", seconds);
        proc::Run(L"C:\\Windows\\System32\\shutdown.exe", args);
        shutdownEnds = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        shutdownActive = true;
    };

    if (ButtonRow("10m", l.Get("sat", "main", "tenM"), [&] { startShutdown(600); })) {
    }
    if (ButtonRow("1h", l.Get("sat", "main", "oneH"), [&] { startShutdown(3600); })) {
    }
    if (ButtonRow("custom", l.Get("sat", "main", "b1"), [&] { startShutdown(mins * 60); })) {
    }
    if (ButtonRow("cancel", l.Get("sat", "main", "b2"), [&] {
            proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-a");
            shutdownActive = false;
        })) {
    }
}

static void DrawPerf() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("perfor", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("perfor", "main", "info").c_str());
    static int pct = 50;
    ImGui::SliderInt("pct", &pct, 1, 100);
    if (ButtonRow("apply", l.Get("perfor", "main", "applyb"), [&] {
            const int throttle = pct;
            auto r = proc::Run(L"powercfg", L"/getactivescheme");
            std::wstring scheme = util::ToWide(r.output);
            auto p = scheme.find(L": ");
            if (p != std::wstring::npos) {
                scheme = scheme.substr(p + 2);
                auto end = scheme.find_last_not_of(L" \r\n");
                if (end != std::wstring::npos) scheme.resize(end + 1);
            }
            wchar_t buf[256];
            swprintf_s(buf, L"/setacvalueindex %s SUB_PROCESSOR PROCTHROTTLEMAX %d", scheme.c_str(),
                       throttle);
            proc::Run(L"powercfg", buf);
            proc::Run(L"powercfg", L"/setactive " + scheme);
        })) {
    }
}

static void DrawAct() {
    PageTitle(Application::Instance().L10n().CatName("act"));
    ImGui::TextWrapped(
        "In the GitHub version, for obvious reasons, the source code of the Windows activation page is not available.\n\n"
        "В GitHub-версии по очевидным причинам исходный код страницы активации Windows недоступен.");
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

    if (g_uwpListLoading.load()) {
        ImGui::TextWrapped("%s", UwpStr(l, "loadinglist", "Loading installed UWP packages...").c_str());
        return;
    }

    std::lock_guard uwpLock(g_uwpMutex);

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
    if (ImGui::BeginCombo("##lang", kLangTags[langIdx])) {
        for (int i = 0; i < 26; ++i) {
            const bool selected = (langIdx == i);
            if (ImGui::Selectable(kLangTags[i], selected)) langIdx = i;
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
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

    if (ImGui::Checkbox(l.Get("ab", "main", "disabletelemetry").c_str(), &s.disableTelemetry))
        s.Save();
    if (ImGui::CollapsingHeader("Telemetry info")) {
        ImGui::TextWrapped("%s", l.Get("ab", "main", "telemetryabt").c_str());
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
