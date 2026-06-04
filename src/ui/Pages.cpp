#include "ui/Pages.h"
#include "ui/UiCommon.h"
#include "ui/Theme.h"
#include "ui/Modals.h"
#include "app/Application.h"
#include "core/AppPaths.h"
#include "core/PerformanceMonitor.h"
#include "core/PresetIO.h"
#include "core/Benchmark.h"
#include "core/JobQueue.h"
#include "core/ProcessMgrUtil.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include "core/StringUtil.h"
#include "core/Wmi.h"
#include <imgui.h>
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

static bool AnyNamespaceMissing() {
    const wchar_t* keys[] = {
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{A0953C92-50DC-43bf-BE83-3742FED03C9C}",
        L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MyComputer\\NameSpace\\{f86fa3ab-70d2-4fc7-9c99-fcbf05467f3a}"};
    for (auto k : keys)
        if (KeyMissing(HKEY_LOCAL_MACHINE, k)) return true;
    return false;
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
    static bool init{};
    if (!init) {
        nonrem = AnyNamespaceMissing();
        DWORD v{};
        if (reg::GetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Hidden",
                          v))
            hidden = v == 1;
        if (reg::GetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                          L"HideFileExt", v))
            ext = v == 0;
        if (reg::GetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"LaunchTo",
                          v))
            pchome = v == 1;
        init = true;
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
    int mins = 30;
    ImGui::SliderInt("mins", &mins, 1, 600);
    if (ButtonRow("10m", l.Get("sat", "main", "tenM"),
                  [] { proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-s -t 600"); }))
    {
    }
    if (ButtonRow("1h", l.Get("sat", "main", "oneH"),
                  [] { proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-s -t 3600"); }))
    {
    }
    if (ButtonRow("custom", l.Get("sat", "main", "b1"), [mins] {
            wchar_t args[64];
            swprintf_s(args, L"-s -t %d", mins * 60);
            proc::Run(L"C:\\Windows\\System32\\shutdown.exe", args);
        })) {
    }
    if (ButtonRow("cancel", l.Get("sat", "main", "b2"),
                  [] { proc::Run(L"C:\\Windows\\System32\\shutdown.exe", L"-a"); }))
    {
    }
}

static void DrawPerf() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("perfor", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("perfor", "main", "info").c_str());
    int pct = 50;
    ImGui::SliderInt("pct", &pct, 1, 100);
    if (ButtonRow("apply", l.Get("perfor", "main", "applyb"), [pct] {
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
                       pct);
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
static void DrawUwp() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("uwp", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("uwp", "main", "info1").c_str());
    static bool u3, u5, u9, u10, u13, u15;
    ToggleRow("u3", l.Get("uwp", "main", "u3"), &u3, nullptr);
    ToggleRow("u5", l.Get("uwp", "main", "u5"), &u5, nullptr);
    ToggleRow("u9", l.Get("uwp", "main", "u9"), &u9, nullptr);
    if (ButtonRow("remove", l.Get("uwp", "main", "b"), [] {
            jobs::JobQueue::Instance().Enqueue([] {
                proc::RunPowerShell(
                    L"Get-AppxPackage *zune* | Remove-AppxPackage; Get-AppxPackage *skypeapp* | Remove-AppxPackage");
            });
        })) {
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

static void DrawMonitoringPanel() {
    auto& l = Application::Instance().L10n();
    const auto snap = perfmon::PerformanceMonitor::Instance().Get();
    ImGui::Text("%s: %.1f%%", l.Get("monitoring", "main", "cpuusage").c_str(), snap.cpuPercent);
    ImGui::Text("%s: %.0f / %.0f MB", l.Get("monitoring", "main", "ram").c_str(), snap.ramUsedMb,
                snap.ramTotalMb);
    ImGui::Text("%s: %.2f MB/s", l.Get("monitoring", "main", "readspeed").c_str(),
                snap.diskReadMbps);
    ImGui::Text("%s: %.2f MB/s", l.Get("monitoring", "main", "writespeed").c_str(),
                snap.diskWriteMbps);
    if (!snap.gpuName.empty()) {
        ImGui::Text("GPU: ");
        ImGui::SameLine();
        TextWide(snap.gpuName);
    }
    static float hist[120]{};
    static int histIdx{};
    hist[histIdx++ % 120] = static_cast<float>(snap.cpuPercent);
    ImGui::PlotLines("CPU", hist, 120, histIdx, nullptr, 0.f, 100.f, ImVec2(-1, 80));
}

static void DrawProcessMgr() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    auto& settings = app.GetSettings();
    PageTitle(l.CatName("procmgr"));

    if (ImGui::Button(l.Get("pmgr", "main", "monitoring").c_str()))
        settings.pmgrShowMonitoring = !settings.pmgrShowMonitoring;
    ImGui::SameLine();
    if (ImGui::Button(l.Get("pmgr", "main", "settings").c_str())) modals::OpenExclusionSettings();
    ImGui::SameLine();
    if (ImGui::Button("MakuYan")) modals::OpenMakuYan();
    if (!app.ExclusiveLayout() &&
        ImGui::Button(l.Get("pmgr", "main", "getfullscr").c_str())) {
        settings.exclusiveMode = true;
        settings.Save();
    }

    if (settings.pmgrShowMonitoring) {
        DrawMonitoringPanel();
        if (ImGui::Button(l.Get("pmgr", "main", "backtoprocess").c_str()))
            settings.pmgrShowMonitoring = false;
        settings.Save();
        return;
    }

    static int tab = 0;
    static std::vector<pmgr::ProcRow> procs;
    static std::vector<pmgr::SvcRow> services;
    static char searchBuf[256]{};
    static DWORD selectedPid = 0;
    static std::wstring selectedSvc;
    static bool needRefresh = true;
    static int lastTab = -1;

    const std::wstring searchW = util::ToWide(searchBuf);
    if (tab != lastTab) {
        needRefresh = true;
        lastTab = tab;
    }

    if (ImGui::BeginTabBar("pmgr_tabs")) {
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

    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##pmgr_search",
                             PmgrStr(l, "filtersearch", "Search by name or PID...").c_str(),
                             searchBuf, sizeof(searchBuf));

    if (ImGui::Button(l.Get("pmgr", "main", "showall").c_str())) needRefresh = true;

    bool groupChanged = false;
    if (ImGui::Checkbox(PmgrStr(l, "group", "Group processes").c_str(), &settings.group))
        groupChanged = true;
    ImGui::SameLine();
    if (ImGui::Checkbox(l.Get("pmgr", "main", "onlyfrozen").c_str(), &settings.onlyFrozen))
        groupChanged = true;
    ImGui::SameLine();
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
    ImGui::SetNextItemWidth(180);
    if (ImGui::Combo("##memfilter", &settings.lastProcessFilterIndex, memFilters,
                     static_cast<int>(std::size(memFilters))))
        groupChanged = true;

    if (groupChanged) settings.Save();

    if (needRefresh || (tab == 0 && procs.empty()) || (tab == 1 && services.empty())) {
        if (tab == 0)
            pmgr::RefreshProcesses(procs);
        else
            pmgr::RefreshServices(services);
        needRefresh = false;
    }

    const std::string yesLbl = l.Def("on");
    const std::string noLbl = l.Def("off");
    const SIZE_T memMin = PmgrMemThresholdMb(settings.lastProcessFilterIndex);

    if (tab == 0) {
        ImGui::SameLine();
        if (ImGui::Button(l.Get("pmgr", "main", "endprocess").c_str()) && selectedPid != 0) {
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, selectedPid);
            if (h) {
                TerminateProcess(h, 0);
                CloseHandle(h);
                selectedPid = 0;
                needRefresh = true;
            }
        }

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

        auto drawProcRow = [&](const pmgr::ProcRow& p) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(p.pid));
            const bool selected = selectedPid == p.pid;
            ImGui::TableNextColumn();
            const std::string rowLabel = util::ToUtf8(p.name) + "##" + std::to_string(p.pid);
            if (ImGui::Selectable(rowLabel.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                selectedPid = p.pid;
            ImGui::TableNextColumn();
            ImGui::Text("%lu", p.pid);
            ImGui::TableNextColumn();
            if (settings.ramMbOnly)
                ImGui::Text("%.0f MB", p.mem / (1024.0 * 1024.0));
            else
                ImGui::Text("%.1f MB", p.mem / (1024.0 * 1024.0));
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

// --- PCI ---
static void DrawPci() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("pci", "main", "label"));
    static bool loadingShown{};
    if (!loadingShown) {
        ImGui::TextWrapped("%s", l.Get("pci", "main", "loading").c_str());
        loadingShown = true;
    }
    std::wstring cpu = wmi::QueryScalar(L"SELECT Name FROM Win32_Processor", L"Name");
    std::wstring ram = wmi::QueryScalar(L"SELECT TotalPhysicalMemory FROM Win32_ComputerSystem",
                                        L"TotalPhysicalMemory");

    ImGui::TextUnformatted(l.Get("pci", "main", "processorname").c_str());
    ImGui::SameLine();
    TextWide(cpu);
    if (!ram.empty()) {
        try {
            const unsigned long long bytes = std::stoull(ram);
            ImGui::Text("%s %.2f GB", l.Get("pci", "main", "ramtotal").c_str(),
                        bytes / (1024.0 * 1024.0 * 1024.0));
        } catch (...) {
        }
    }
    auto gpus = wmi::QueryList(L"SELECT Name FROM Win32_VideoController", L"Name");
    for (auto& g : gpus) {
        ImGui::Bullet();
        ImGui::SameLine();
        TextWide(g);
    }

    ImGui::Separator();
    ImGui::TextUnformatted(l.Get("pci", "main", "benchtitle").c_str());

    static std::atomic<bool> benchRunning{false};
    static std::string benchResult;
    static std::mutex benchMutex;
    if (benchResult.empty()) benchResult = l.Get("pci", "main", "benchtip");

    auto startBench = [&](bool multi) {
        if (benchRunning.exchange(true)) return;

        const std::string running =
            multi ? l.Get("pci", "main", "running_multicore") : l.Get("pci", "main", "running");
        const std::string done =
            multi ? l.Get("pci", "main", "test1multi") : l.Get("pci", "main", "test1");
        const std::string scoreLbl = l.Get("pci", "main", "test2");
        const std::string opsLbl = l.Get("pci", "main", "test3");

        {
            std::lock_guard lock(benchMutex);
            benchResult = running;
        }

        jobs::JobQueue::Instance().Enqueue([multi, done, scoreLbl, opsLbl]() {
            try {
                const auto r = bench::Run(multi);
                const std::string line =
                    done + " " + scoreLbl + " " + FormatScore(r.score) + " " + opsLbl;
                std::lock_guard lock(benchMutex);
                benchResult = line;
            } catch (...) {
                std::lock_guard lock(benchMutex);
                benchResult = done + " (error)";
            }
            benchRunning = false;
        });
    };

    const bool busy = benchRunning.load();
    if (busy) ImGui::BeginDisabled();
    if (ImGui::Button(l.Get("pci", "main", "benchbutton").c_str())) startBench(false);
    ImGui::SameLine();
    if (ImGui::Button(l.Get("pci", "main", "benchbutton2").c_str())) startBench(true);
    ImGui::SameLine();
    if (ImGui::Button(l.Get("pci", "main", "lookresulbutton").c_str()))
        proc::OpenUrl(L"https://adderly.top/makubench");
    if (busy) ImGui::EndDisabled();

    {
        std::lock_guard lock(benchMutex);
        ImGui::TextWrapped("%s", benchResult.c_str());
    }

    auto saveReport = [&]() {
        std::wstring path = util::GetExeDirectory() + L"\\pci_export.txt";
        std::wofstream out(path);
        out << L"CPU: " << cpu << L"\nRAM: " << ram << L"\n";
        for (auto& g : gpus) out << L"GPU: " << g << L"\n";
        MessageBoxW(Application::Instance().Hwnd(),
                    util::ToWide(l.Get("pci", "main", "save_done")).c_str(), L"MakuTweaker",
                    MB_OK);
    };
    if (ImGui::Button(l.Get("pci", "main", "save").c_str())) saveReport();
    ImGui::SameLine();
    if (ImGui::Button(l.Get("pci", "main", "clipboard").c_str())) {
        std::wstring clip = L"CPU: " + cpu + L"\nRAM: " + ram;
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            const size_t bytes = (clip.size() + 1) * sizeof(wchar_t);
            HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
            if (mem) {
                memcpy(GlobalLock(mem), clip.c_str(), bytes);
                GlobalUnlock(mem);
                SetClipboardData(CF_UNICODETEXT, mem);
            }
            CloseClipboard();
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F5) ||
        (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)))
        saveReport();
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
        ui::ApplyTheme(s.theme);
        ui::SyncDwmDark(app.Hwnd(), false);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(l.Get("ab", "main", "d").c_str(), themeDark)) {
        themeDark = true;
        s.theme = "Dark";
        s.Save();
        ui::ApplyTheme(s.theme);
        ui::SyncDwmDark(app.Hwnd(), true);
    }
    ImGui::Separator();
    ImGui::Text("MakuTweaker 5.8 (C++/ImGui)");
    ImGui::Text("Mark Adderly");
    if (ImGui::Button("GitHub"))
        proc::OpenUrl(L"https://github.com/MarkAdderly/MakuTweaker");
    if (ImGui::Button("Donate")) proc::OpenUrl(L"https://adderly.top/mt");

    bool winr = apppaths::WinRAliasesEnabled();
    if (ImGui::Checkbox(l.Get("ab", "main", "winr").c_str(), &winr)) apppaths::SetWinRAliases(winr);
    ImGui::TextWrapped("%s", l.Get("ab", "main", "winrinfo").c_str());

    if (ImGui::Checkbox(l.Get("ab", "main", "disabletelemetry").c_str(), &s.disableTelemetry))
        s.Save();

    ImGui::Checkbox(l.Get("pmgr", "main", "modeset").c_str(), &s.autoStartExclusive);

    ImGui::Separator();
    ImGui::TextUnformatted(l.Get("ab", "main", "cfg_title").c_str());
    if (ImGui::Button(l.Get("ab", "main", "cfg_save").c_str())) {
        std::wstring path = util::GetExeDirectory() + L"\\preset.mktw";
        std::wstring err;
        if (preset::ExportSettings(path, &err))
            MessageBoxW(app.Hwnd(), util::ToWide(l.Get("ab", "main", "cfg_svsuccess")).c_str(),
                        L"MakuTweaker", MB_OK);
        else
            MessageBoxW(app.Hwnd(), err.c_str(), L"MakuTweaker", MB_OK | MB_ICONERROR);
    }
    ImGui::SameLine();
    if (ImGui::Button(l.Get("ab", "main", "cfg_import").c_str())) {
        std::wstring path = util::GetExeDirectory() + L"\\preset.mktw";
        std::wstring err;
        if (preset::ImportSettings(path, &err))
            MessageBoxW(app.Hwnd(), util::ToWide(l.Get("ab", "main", "cfg_ldsuccess")).c_str(),
                        L"MakuTweaker", MB_OK);
        else
            MessageBoxW(app.Hwnd(), err.c_str(), L"MakuTweaker", MB_OK | MB_ICONERROR);
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
    case app::PageId::Settings: DrawSettings(); break;
    default: break;
    }
}

} // namespace maku::ui::pages
