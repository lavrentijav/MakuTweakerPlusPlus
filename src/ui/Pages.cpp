#include "ui/Pages.h"
#include "ui/UiCommon.h"
#include "ui/Theme.h"
#include "app/Application.h"
#include "core/Benchmark.h"
#include "core/JobQueue.h"
#include "core/ProcessMgrUtil.h"
#include "core/OsUtil.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include "core/StringUtil.h"
#include "core/SysInfo.h"
#include "core/PciCollector.h"
#include "core/Clipboard.h"
#include "core/SystemReport.h"
#include "core/Preset.h"
#include "core/LogFile.h"
#include "platform/MetricsService.h"
#include "core/PendingUi.h"
#include "core/Tweaks.h"
#include "ui/Extras.h"
#include "ui/MonitorPage.h"
#include <atomic>
#include <nlohmann/json.hpp>
#include "core/Wmi.h"
#include <imgui.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>
#include <filesystem>
#include <functional>
#include <vector>
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

#include "core/Languages.h"

namespace maku::ui::pages {
namespace {

using app::Application;

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
                  [](bool on) { DeleteNamespaceKeys(on); }, true);
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
    if (ButtonRow(
            "fix", l.Get("expl", "main", "e8b"),
            [] {
                reg::DeleteKeyTree(
                    HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\DelegateFolders\\{F5FB2C77-0E2F-4A16-A381-3E560C68BC83}");
                reg::DeleteKeyTree(
                    HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Desktop\\NameSpace\\DelegateFolders\\{F5FB2C77-0E2F-4A16-A381-3E560C68BC83}");
            },
            true))
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

    ui::DrawSaveFooter(
        "expl",
        [&]() -> bool {
            reg::SetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"Hidden",
                          hidden ? 1u : 0u);
            reg::SetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced",
                          L"HideFileExt", ext ? 0u : 1u);
            reg::SetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced", L"LaunchTo",
                          pchome ? 1u : 2u);
            reg::SetDword(HKEY_CURRENT_USER,
                          L"SOFTWARE\\Classes\\CLSID\\{e88865ea-0e1c-4e20-9aa6-edcd0212c87c}",
                          L"System.IsPinnedToNameSpaceTree", gallery ? 0u : 1u);
            reg::SetDword(
                HKEY_CURRENT_USER,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideDesktopIcons\\NewStartPanel",
                L"{20D04FE0-3AEA-1069-A2D8-08002B30309D}", showpc ? 0u : 1u);
            if (shortcut)
                reg::SetString(HKEY_CURRENT_USER,
                               L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\NamingTemplates",
                               L"ShortcutNameTemplate", L"%s.lnk");
            else
                reg::DeleteKeyTree(HKEY_CURRENT_USER,
                                   L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\NamingTemplates");
            if (os::HasAdminRights()) DeleteNamespaceKeys(nonrem);
            return os::HasAdminRights() || !nonrem;
        },
        false);
}

static void DrawWindowsUpdate() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("wu", "main", "label"));
    static bool wu1, wu3, wu5, wu6;
    static bool wuInit{};
    if (!wuInit) {
        DWORD v{};
        if (reg::GetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                          L"DisableWindowsUpdateAccess", v))
            wu1 = v != 0;
        if (reg::GetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                          L"ExcludeWUDriversInQualityUpdate", v))
            wu3 = v != 0;
        wuInit = true;
    }
    ToggleRow(
        "wu1", l.Get("wu", "main", "wu1"), &wu1,
        [](bool on) {
            if (on) {
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                              L"DoNotConnectToWindowsUpdateInternetLocations", 1);
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                              L"DisableWindowsUpdateAccess", 1);
                jobs::JobQueue::Instance().Enqueue([] {
                    proc::RunAdmin(L"net", L"stop wuauserv");
                    proc::RunAdmin(L"sc", L"config wuauserv start= disabled");
                });
            } else {
                reg::DeleteKeyTree(HKEY_LOCAL_MACHINE,
                                   L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate");
                jobs::JobQueue::Instance().Enqueue([] {
                    proc::RunAdmin(L"sc", L"config wuauserv start= demand");
                    proc::RunAdmin(L"net", L"start wuauserv");
                });
            }
        },
        true);
    ToggleRow(
        "wu3", l.Get("wu", "main", "wu3"), &wu3,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                          L"ExcludeWUDriversInQualityUpdate", on ? 1u : 0u);
        },
        true);
    if (ButtonRow(
            "wu4", l.Get("wu", "main", "wu4"),
            [] {
                jobs::JobQueue::Instance().Enqueue([] {
                    proc::RunAdmin(L"net", L"stop wuauserv");
                    proc::RunHiddenAdmin("net stop bits && net stop cryptsvc");
                    proc::RunHiddenAdmin(
                        "ren C:\\Windows\\SoftwareDistribution SoftwareDistribution.old 2>nul");
                    proc::RunAdmin(L"net", L"start wuauserv");
                });
            },
            true))
        Application::Instance().NotifyReboot(1);
    ToggleRow(
        "wu5", l.Get("wu", "main", "wu5"), &wu5,
        [](bool on) { proc::RunAdmin(L"net", on ? L"stop wuauserv" : L"start wuauserv"); }, true);
    ToggleRow(
        "wu6", l.Get("wu", "main", "wu6"), &wu6,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ReserveManager",
                          L"ShippedWithReserves", on ? 0u : 1u);
        },
        true);

    ui::DrawSaveFooter(
        "wu",
        [&]() -> bool {
            if (!os::HasAdminRights()) return false;
            if (wu1) {
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                              L"DoNotConnectToWindowsUpdateInternetLocations", 1);
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                              L"DisableWindowsUpdateAccess", 1);
            } else {
                reg::DeleteKeyTree(HKEY_LOCAL_MACHINE,
                                   L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate");
            }
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Policies\\Microsoft\\Windows\\WindowsUpdate",
                          L"ExcludeWUDriversInQualityUpdate", wu3 ? 1u : 0u);
            proc::RunAdmin(L"net", wu5 ? L"stop wuauserv" : L"start wuauserv");
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\ReserveManager",
                          L"ShippedWithReserves", wu6 ? 0u : 1u);
            return true;
        },
        true);
}

static void DrawSysAndRec() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("sr", "main", "label"));
    static bool telemetry, uac, hybern, smartscreen, bing, sticky, bitlocker, coreisol, chkdsk;
    static bool srInit{};
    if (!srInit) {
        DWORD v{};
        if (reg::GetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
                          L"AllowTelemetry", v))
            telemetry = v == 0;
        if (reg::GetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA",
                          v))
            uac = v == 0;
        if (reg::GetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                          L"EnableSmartScreen", v))
            smartscreen = v == 0;
        if (reg::GetDword(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Windows\\Explorer",
                          L"DisableSearchBoxSuggestions", v))
            bing = v != 0;
        std::wstring flags;
        if (reg::GetString(HKEY_CURRENT_USER, L"Control Panel\\Accessibility\\StickyKeys", L"Flags",
                           flags))
            sticky = flags == L"506" || flags == L"58";
        if (reg::GetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\FVE",
                          L"DisableDeviceEncryption", v))
            bitlocker = v != 0;
        if (reg::GetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                          L"EnableVirtualizationBasedSecurity", v))
            coreisol = v == 0;
        if (reg::GetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                          L"AutoChkTimeOut", v))
            chkdsk = v >= 60;
        srInit = true;
    }
    ToggleRow(
        "telemetry", l.Get("sr", "main", "telemetry"), &telemetry,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", L"AllowTelemetry",
                          on ? 0u : 3u);
        },
        true);
    ToggleRow(
        "uac", l.Get("sr", "main", "uac"), &uac,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                          L"EnableLUA", on ? 0u : 1u);
        },
        true);
    ToggleRow(
        "hybern", l.Get("sr", "main", "hybern"), &hybern,
        [](bool on) {
            proc::RunHiddenAdmin(on ? "powercfg /hibernate off" : "powercfg /hibernate on");
        },
        true);
    ToggleRow(
        "smartscreen", l.Get("sr", "main", "smartscreen"), &smartscreen,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                          L"EnableSmartScreen", on ? 0u : 1u);
        },
        true);
    ToggleRow(
        "bing", l.Get("sr", "main", "bing"), &bing,
        [](bool on) {
            reg::SetDword(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Windows\\Explorer",
                          L"DisableSearchBoxSuggestions", on ? 1u : 0u);
        });
    ToggleRow(
        "sticky", l.Get("sr", "main", "sticky"), &sticky,
        [](bool on) {
            reg::SetString(HKEY_CURRENT_USER, L"Control Panel\\Accessibility\\StickyKeys", L"Flags",
                           on ? L"506" : L"510");
        });
    ToggleRow(
        "bitlocker", l.Get("sr", "main", "bitlocker"), &bitlocker,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\FVE",
                          L"DisableDeviceEncryption", on ? 1u : 0u);
        },
        true);
    ToggleRow(
        "coreisol", l.Get("sr", "main", "coreisol"), &coreisol,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                          L"EnableVirtualizationBasedSecurity", on ? 0u : 1u);
        },
        true);
    ToggleRow(
        "chkdsk", l.Get("sr", "main", "chkdsk"), &chkdsk,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                          L"AutoChkTimeOut", on ? 60u : 8u);
        },
        true);
    if (ButtonRow(
            "sfc", l.Get("sr", "main", "sfclabel"),
            [] {
                jobs::JobQueue::Instance().Enqueue([] {
                    proc::RunAdmin(L"sfc", L"/scannow");
                    Application::Instance().NotifyReboot(3);
                });
            },
            true)) {
    }
    if (ButtonRow(
            "dism", l.Get("sr", "main", "dismlabel"),
            [] {
                jobs::JobQueue::Instance().Enqueue([] {
                    proc::RunAdmin(L"dism", L"/Online /Cleanup-Image /RestoreHealth");
                });
            },
            true)) {
    }
    if (ButtonRow("temp", l.Get("sr", "main", "templabel"),
                  [] { proc::RunHidden("del /q /f /s %TEMP%\\* 2>nul"); })) {
    }
    if (ButtonRow("battery", l.Get("sr", "main", "batterylabel"),
                  [] { proc::RunHidden("powercfg /batteryreport"); })) {
    }

    ui::DrawSaveFooter(
        "sr",
        [&]() -> bool {
            if (!os::HasAdminRights()) return false;
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
                          L"AllowTelemetry", telemetry ? 0u : 3u);
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                          L"EnableLUA", uac ? 0u : 1u);
            proc::RunHiddenAdmin(hybern ? "powercfg /hibernate off" : "powercfg /hibernate on");
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                          L"EnableSmartScreen", smartscreen ? 0u : 1u);
            reg::SetDword(HKEY_CURRENT_USER, L"Software\\Policies\\Microsoft\\Windows\\Explorer",
                          L"DisableSearchBoxSuggestions", bing ? 1u : 0u);
            reg::SetString(HKEY_CURRENT_USER, L"Control Panel\\Accessibility\\StickyKeys", L"Flags",
                           sticky ? L"506" : L"510");
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\FVE",
                          L"DisableDeviceEncryption", bitlocker ? 1u : 0u);
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                          L"EnableVirtualizationBasedSecurity", coreisol ? 0u : 1u);
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager",
                          L"AutoChkTimeOut", chkdsk ? 60u : 8u);
            return true;
        },
        true);
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
    ToggleRow(
        "verbose", l.Get("per", "main", "verbose"), &verbose,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE,
                          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                          L"verbosestatus", on ? 1u : 0u);
        },
        true);
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

    ui::DrawSaveFooter(
        "per",
        [&]() -> bool {
            reg::SetDword(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          L"AppsUseLightTheme", dark ? 0u : 1u);
            reg::SetDword(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          L"SystemUsesLightTheme", dark ? 0u : 1u);
            reg::SetDword(HKEY_CURRENT_USER,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                          L"EnableTransparency", transparency ? 0u : 1u);
            if (os::GetWindowsBuild() >= 22000)
                reg::SetDword(HKEY_CURRENT_USER,
                              L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Advanced\\TaskbarDeveloperSettings",
                              L"TaskbarEndTask", endtask ? 1u : 0u);
            if (os::GetWindowsBuild() >= 22621) {
                if (oldcont)
                    reg::CreateKey(HKEY_CURRENT_USER,
                                   L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}\\InprocServer32");
                else
                    reg::DeleteKeyTree(HKEY_CURRENT_USER,
                                       L"Software\\Classes\\CLSID\\{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}");
            }
            if (!os::HasAdminRights() && verbose) return false;
            if (os::HasAdminRights())
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                              L"verbosestatus", verbose ? 1u : 0u);
            return true;
        },
        false);
}

static void DrawAdvanced() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.CatName("adv"));
    static bool vbs, ttl, disindex, swap;
    ToggleRow(
        "vbs", l.Get("adv", "main", "vbs"), &vbs,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                          L"EnableVirtualizationBasedSecurity", on ? 0u : 1u);
        },
        true);
    ToggleRow(
        "ttl", l.Get("adv", "main", "ttl"), &ttl,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                          L"DefaultTTL", on ? 65u : 128u);
        },
        true);
    ToggleRow(
        "disindex", l.Get("adv", "main", "index_title"), &disindex,
        [](bool on) {
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\WSearch", L"Start",
                          on ? 4u : 2u);
            if (on) proc::RunAdmin(L"net", L"stop WSearch");
            else proc::RunAdmin(L"net", L"start WSearch");
        },
        true);
    if (ButtonRow(
            "edge", l.Get("adv", "main", "deledge_btn"),
            [] {
                jobs::JobQueue::Instance().Enqueue([] {
                    proc::RunPowerShell(
                        L"Get-AppxPackage *Microsoft.MicrosoftEdge* | Remove-AppxPackage -ErrorAction "
                        L"SilentlyContinue");
                });
            },
            true)) {
    }

    ui::DrawSaveFooter(
        "adv",
        [&]() -> bool {
            if (!os::HasAdminRights()) return false;
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard",
                          L"EnableVirtualizationBasedSecurity", vbs ? 0u : 1u);
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
                          L"DefaultTTL", ttl ? 65u : 128u);
            reg::SetDword(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\WSearch", L"Start",
                          disindex ? 4u : 2u);
            proc::RunAdmin(L"net", disindex ? L"stop WSearch" : L"start WSearch");
            return true;
        },
        true);

    ImGui::Separator();
    extras::DrawAutomationPanel();
    extras::DrawNetworkPanel();
    extras::DrawWingetPanel();
    extras::DrawShellPanel();
    extras::DrawRollbackPanel();
}

static void DrawComponents() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("compon", "main", "label"));
    if (ButtonRow(
            "dp", l.Get("compon", "main", "directplay"),
            [] { proc::RunAdmin(L"dism", L"/online /enable-feature /featurename:DirectPlay /all"); },
            true)) {
    }
    if (ButtonRow(
            "netfx", l.Get("compon", "main", "framework"),
            [] { proc::RunAdmin(L"dism", L"/online /enable-feature /featurename:NetFx3 /all"); }, true)) {
    }
    if (ButtonRow(
            "pv", l.Get("compon", "main", "photoviewer"),
            [] {
                proc::RunHiddenAdmin(
                    "reg add \"HKLM\\SOFTWARE\\Microsoft\\Windows Photo Viewer\\Capabilities\\FileAssociations\" /v .jpg /t REG_SZ /d PhotoViewer.FileAssoc.Tiff /f");
            },
            true)) {
    }
    if (ButtonRow(
            "gpedit", l.Get("compon", "main", "gpedit"),
            [] {
                proc::RunPowerShell(
                    L"foreach ($i in 'gpedit','fde','gaudit','ppe') { dism /online /add-capability /CapabilityName:Rsat.$i.Tools~~~~0.0.1.0 }");
            },
            true)) {
    }
    if (ButtonRow(
            "hyperv", l.Get("compon", "main", "forcedis"),
            [] { proc::RunAdmin(L"bcdedit", L"/set hypervisorlaunchtype off"); }, true)) {
    }
    if (ButtonRow(
            "winsxs", l.Get("compon", "main", "winsxs"),
            [] {
                proc::RunAdmin(L"dism",
                               L"/online /Cleanup-Image /StartComponentCleanup /ResetBase");
            },
            true)) {
    }
}

static void DrawQuickSet() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("quick", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("quick", "main", "info").c_str());
    if (ButtonRow(
            "q1", l.Get("quick", "main", "b"),
            [] {
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
                              L"AllowTelemetry", 0);
                proc::RunHiddenAdmin("powercfg /hibernate off");
            },
            true)) {
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

static std::wstring ActivePowerSchemeGuid() {
    const auto r = proc::Run(L"powercfg", L"/getactivescheme");
    const std::wstring out = util::ToWide(r.output);
    const size_t guidPos = out.find(L"GUID:");
    if (guidPos == std::wstring::npos) return L"SCHEME_CURRENT";
    size_t start = guidPos + 5;
    while (start < out.size() && (out[start] == L' ' || out[start] == L':')) ++start;
    size_t end = start;
    while (end < out.size() && out[end] != L'\r' && out[end] != L'\n') ++end;
    std::wstring scheme = out.substr(start, end - start);
    while (!scheme.empty() && iswspace(scheme.back())) scheme.pop_back();
    return scheme.empty() ? L"SCHEME_CURRENT" : scheme;
}

// PROCTHROTTLEMAX (5935ec) — max processor state 0–100%. Do not use 5935ed (efficiency class 1).
static constexpr wchar_t kProcThrottleMaxGuid[] =
    L"bc5038f7-23e0-4960-96da-33abaf5935ec";

static bool IsPowerCfgCurrentValueLine(const std::wstring& line) {
    if (line.find(L"0x") == std::wstring::npos) return false;
    // Skip min/max/increment metadata lines (EN + RU).
    if (line.find(L"Minimum Possible") != std::wstring::npos ||
        line.find(L"Maximum Possible") != std::wstring::npos ||
        line.find(L"Possible Settings increment") != std::wstring::npos)
        return false;
    if (line.find(L"инимальн") != std::wstring::npos || line.find(L"аксимальн") != std::wstring::npos)
        return false;
    if (line.find(L"риращение") != std::wstring::npos) return false;
    return line.find(L"urrent") != std::wstring::npos || line.find(L"екущ") != std::wstring::npos;
}

static int HexValueOnLine(const std::wstring& line) {
    const size_t hexPos = line.find(L"0x");
    if (hexPos == std::wstring::npos) return -1;
    return static_cast<int>(wcstol(line.c_str() + hexPos + 2, nullptr, 16));
}

static int NormalizeCpuThrottlePercent(int raw) {
    if (raw < 0) return -1;
    if (raw <= 100) return raw;
    if (raw <= 10000) return (raw + 50) / 100;
    if (raw <= 500) return (raw * 100 + 250) / 500;
    return -1;
}

static int QueryCpuThrottlePercent(const std::wstring& scheme) {
    wchar_t args[512];
    swprintf_s(args, L"/query %s SUB_PROCESSOR PROCTHROTTLEMAX", scheme.c_str());
    auto r = proc::Run(L"powercfg", args);
    std::wstring out = util::ToWide(r.output);
    if (out.find(kProcThrottleMaxGuid) == std::wstring::npos &&
        out.find(L"PROCTHROTTLEMAX") == std::wstring::npos) {
        swprintf_s(args, L"/query %s 54533251-82be-4824-96c1-47b60b740d00 %s", scheme.c_str(),
                   kProcThrottleMaxGuid);
        r = proc::Run(L"powercfg", args);
        out = util::ToWide(r.output);
    }

    int ac = -1, dc = -1;
    std::wstring line;
    for (size_t i = 0; i <= out.size(); ++i) {
        const wchar_t ch = (i < out.size()) ? out[i] : L'\n';
        if (ch != L'\n' && ch != L'\r') {
            line += ch;
            continue;
        }
        if (!line.empty() && IsPowerCfgCurrentValueLine(line)) {
            const int v = HexValueOnLine(line);
            if (v >= 0) {
                const bool isDc = line.find(L" DC") != std::wstring::npos ||
                                  line.find(L"батар") != std::wstring::npos ||
                                  line.find(L"Батар") != std::wstring::npos;
                if (isDc)
                    dc = v;
                else
                    ac = v;
            }
        }
        line.clear();
    }

    int raw = std::max(ac, dc);
    if (raw < 0) {
        int last = -1, secondLast = -1;
        for (size_t pos = 0; pos < out.size();) {
            const size_t hexPos = out.find(L"0x", pos);
            if (hexPos == std::wstring::npos) break;
            secondLast = last;
            last = static_cast<int>(wcstol(out.c_str() + hexPos + 2, nullptr, 16));
            pos = hexPos + 2;
        }
        raw = std::max(last, secondLast);
    }
    return NormalizeCpuThrottlePercent(raw);
}

static bool ApplyCpuThrottle(int pct) {
    if (!os::HasAdminRights()) return false;
    const std::wstring scheme = ActivePowerSchemeGuid();
    proc::RunAdmin(L"powercfg",
                   L"/attributes SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX -ATTRIB_HIDE");
    wchar_t buf[320];
    swprintf_s(buf, L"/setdcvalueindex %s SUB_PROCESSOR PROCTHROTTLEMAX %d", scheme.c_str(), pct);
    proc::RunAdmin(L"powercfg", buf);
    swprintf_s(buf, L"/setacvalueindex %s SUB_PROCESSOR PROCTHROTTLEMAX %d", scheme.c_str(), pct);
    proc::RunAdmin(L"powercfg", buf);
    swprintf_s(buf, L"/setactive %s", scheme.c_str());
    proc::RunAdmin(L"powercfg", buf);
    logf::Info("CPU throttle PROCTHROTTLEMAX set to " + std::to_string(pct) + "%");
    return true;
}

static void DrawPerf() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("perfor", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("perfor", "main", "info").c_str());

    static int pct = 50;
    static bool pctLoaded = false;
    if (!pctLoaded) {
        proc::Run(L"powercfg",
                  L"/attributes SCHEME_CURRENT SUB_PROCESSOR PROCTHROTTLEMAX -ATTRIB_HIDE");
        const bool stagedPct = pending::GetInt("perf_pct").has_value();
        if (auto staged = pending::GetInt("perf_pct")) {
            if (*staged >= 1 && *staged <= 100)
                pct = *staged;
            else
                pending::Remove("perf_pct");
        }
        const int cur = QueryCpuThrottlePercent(ActivePowerSchemeGuid());
        if (cur >= 1 && cur <= 100 && !stagedPct) pct = cur;
        pct = std::clamp(pct, 1, 100);
        pctLoaded = true;
    }

    pct = std::clamp(pct, 1, 100);
    ImGui::SetNextItemWidth(-1.f);
    ImGui::SliderInt("##pct", &pct, 1, 100, "%d%%");
    const float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button(l.Get("perfor", "main", "minb").c_str(), ImVec2(btnW, 0))) pct = 5;
    ImGui::SameLine();
    if (ImGui::Button(l.Get("perfor", "main", "maxb").c_str(), ImVec2(-1.f, 0))) pct = 100;

    ui::DrawSaveFooter(
        "perf",
        [&]() {
            const bool ok = ApplyCpuThrottle(pct);
            if (ok) pending::Remove("perf_pct");
            return ok;
        },
        true, [&]() { pending::StageInt("perf_pct", pct); });
}

static void DrawAct() {
    PageTitle(Application::Instance().L10n().CatName("act"));
    ImGui::TextWrapped(
        "In the GitHub version, for obvious reasons, the source code of the Windows activation page is not available.\n\n"
        "В GitHub-версии по очевидным причинам исходный код страницы активации Windows недоступен.");
}

// --- UWP ---
static std::vector<std::wstring> BuildUwpRemovalPatterns(bool u3, bool u5, bool u9, bool u10,
                                                           bool u13, bool u15) {
    std::vector<std::wstring> patterns;
    auto add = [&](const wchar_t* pattern) { patterns.emplace_back(pattern); };
    if (u3) {
        add(L"*communicationsapps*");
        add(L"*windowscommunicationsapps*");
        add(L"*microsoft.windowscommunicationsapps*");
        add(L"*microsoft.messaging*");
    }
    if (u5) {
        add(L"*gethelp*");
        add(L"*getstarted*");
    }
    if (u9) {
        add(L"*yourphone*");
        add(L"*microsoft.yourphone*");
    }
    if (u10) {
        add(L"*windowsmaps*");
        add(L"*maps*");
    }
    if (u13) {
        add(L"*people*");
        add(L"*outlook*");
        add(L"*outlookforwindows*");
        add(L"*officehub*");
        add(L"*microsoftofficehub*");
    }
    if (u15) {
        add(L"*zune*");
        add(L"*groove*");
        add(L"*movies*");
        add(L"*mediaplayer*");
    }
    return patterns;
}

namespace {
std::atomic<bool> g_uwpRemovalRunning{false};
std::atomic<int> g_uwpRemovalResult{-1}; // -1 none, 0 fail, 1 ok
} // namespace

static bool StartUwpRemovalAsync(const std::vector<std::wstring>& patterns) {
    if (patterns.empty() || g_uwpRemovalRunning.load()) return false;
    g_uwpRemovalRunning = true;
    g_uwpRemovalResult = -1;
    const std::wstring started =
        util::ToWide(Application::Instance().L10n().Get("uwp", "status", "started"));
    jobs::JobQueue::Instance().Enqueue([patterns, started]() {
        jobs::JobQueue::Instance().SetProgress(started, 0.f);
        const bool ok = tweaks::RemoveUwpPackages(
            patterns, [](const int cur, const int total, const std::wstring& name) {
                wchar_t buf[512]{};
                swprintf_s(buf, L"%d / %d — %ls", cur, total, name.c_str());
                const float v = total > 0 ? static_cast<float>(cur) / static_cast<float>(total)
                                          : 0.f;
                jobs::JobQueue::Instance().SetProgress(buf, v);
            });
        jobs::JobQueue::Instance().SetProgress(L"", -1.f);
        g_uwpRemovalResult = ok ? 1 : 0;
        g_uwpRemovalRunning = false;
        logf::Info(std::string("Page save 'uwp': ") + (ok ? "OK" : "FAILED") +
                   (os::HasAdminRights() ? " (admin)" : ""));
    });
    return true;
}

static void StageUwpSelection(bool u3, bool u5, bool u9, bool u10, bool u13, bool u15) {
    pending::StageBool("u3", u3);
    pending::StageBool("u5", u5);
    pending::StageBool("u9", u9);
    pending::StageBool("u10", u10);
    pending::StageBool("u13", u13);
    pending::StageBool("u15", u15);
}

static void DrawUwp() {
    auto& l = Application::Instance().L10n();
    PageTitle(l.Get("uwp", "main", "label"));
    ImGui::TextWrapped("%s", l.Get("uwp", "main", "info1").c_str());
    static bool u3, u5, u9, u10, u13, u15;
    ToggleRow("u3", l.Get("uwp", "main", "u3"), &u3, nullptr, false, true);
    ToggleRow("u5", l.Get("uwp", "main", "u5"), &u5, nullptr, false, true);
    ToggleRow("u9", l.Get("uwp", "main", "u9"), &u9, nullptr, false, true);
    ToggleRow("u10", l.Get("uwp", "main", "u10"), &u10, nullptr, false, true);
    ToggleRow("u13", l.Get("uwp", "main", "u13"), &u13, nullptr, false, true);
    ToggleRow("u15", l.Get("uwp", "main", "u15"), &u15, nullptr, false, true);

    const int done = g_uwpRemovalResult.exchange(-1);
    if (done >= 0) ui::NotifyPageSaved("uwp", done == 1);

    if (g_uwpRemovalRunning.load()) {
        ImGui::Spacing();
        float pv = jobs::JobQueue::Instance().ProgressValue();
        if (pv < 0.f) pv = 0.f;
        ImGui::ProgressBar(pv, ImVec2(-1.f, 0.f));
        const std::wstring& pt = jobs::JobQueue::Instance().ProgressText();
        if (!pt.empty()) ImGui::TextWrapped("%ls", pt.c_str());
        ImGui::BeginDisabled();
    }

    ui::DrawSaveFooter(
        "uwp",
        [&]() {
            const auto patterns = BuildUwpRemovalPatterns(u3, u5, u9, u10, u13, u15);
            return StartUwpRemovalAsync(patterns);
        },
        true, [&]() { StageUwpSelection(u3, u5, u9, u10, u13, u15); }, false);

    if (g_uwpRemovalRunning.load()) ImGui::EndDisabled();
}

// --- Process Manager ---
static std::string PmgrStr(const l10n::Localization& l, const char* key, const char* enFallback) {
    const std::string v = l.Get("pmgr", "main", key);
    return v == key ? enFallback : v;
}

static void SortProcIndices(std::vector<size_t>& idx, const std::vector<pmgr::ProcRow>& procs,
                            const ImGuiTableSortSpecs* specs) {
    if (!specs || specs->SpecsCount == 0 || idx.size() < 2) return;
    const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
    const bool asc = spec.SortDirection == ImGuiSortDirection_Ascending;
    std::sort(idx.begin(), idx.end(), [&](const size_t ia, const size_t ib) {
        const auto& a = procs[ia];
        const auto& b = procs[ib];
        int cmp = 0;
        switch (spec.ColumnIndex) {
        case 0:
            cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
            break;
        case 1:
            cmp = (a.pid < b.pid) ? -1 : (a.pid > b.pid ? 1 : 0);
            break;
        case 2:
            cmp = (a.mem < b.mem) ? -1 : (a.mem > b.mem ? 1 : 0);
            break;
        case 3:
            cmp = (a.critical == b.critical) ? 0 : (a.critical ? 1 : -1);
            break;
        case 4:
            cmp = _wcsicmp(a.path.c_str(), b.path.c_str());
            break;
        default:
            cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
            break;
        }
        if (cmp == 0) cmp = (a.pid < b.pid) ? -1 : (a.pid > b.pid ? 1 : 0);
        return asc ? cmp < 0 : cmp > 0;
    });
}

static void SortSvcIndices(std::vector<size_t>& idx, const std::vector<pmgr::SvcRow>& svcs,
                           const ImGuiTableSortSpecs* specs) {
    if (!specs || specs->SpecsCount == 0 || idx.size() < 2) return;
    const ImGuiTableColumnSortSpecs& spec = specs->Specs[0];
    const bool asc = spec.SortDirection == ImGuiSortDirection_Ascending;
    std::sort(idx.begin(), idx.end(), [&](const size_t ia, const size_t ib) {
        const auto& a = svcs[ia];
        const auto& b = svcs[ib];
        int cmp = 0;
        switch (spec.ColumnIndex) {
        case 0:
            cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
            break;
        case 1:
            cmp = _wcsicmp(a.displayName.c_str(), b.displayName.c_str());
            break;
        case 2:
            cmp = (a.state < b.state) ? -1 : (a.state > b.state ? 1 : 0);
            break;
        case 3:
            cmp = (a.startType < b.startType) ? -1 : (a.startType > b.startType ? 1 : 0);
            break;
        case 4:
            cmp = (a.critical == b.critical) ? 0 : (a.critical ? 1 : -1);
            break;
        default:
            cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
            break;
        }
        if (cmp == 0) cmp = _wcsicmp(a.name.c_str(), b.name.c_str());
        return asc ? cmp < 0 : cmp > 0;
    });
}

static void DrawProcessMgr() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    auto& settings = app.GetSettings();
    PageTitle(l.CatName("procmgr"));

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
    ImGui::SameLine();
    if (ImGui::Checkbox(PmgrStr(l, "showsystem", "Show system processes").c_str(),
                          &settings.showSystemProcesses))
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

    const std::string endProcLbl = l.Get("pmgr", "main", "endprocess");
    const std::string endGroupLbl = PmgrStr(l, "endprocessgroup", "End all with this name");
    const std::string exclLbl = l.Get("pmgr", "main", "excl");
    const std::string openLocLbl = PmgrStr(l, "openlocation", "Open file location");
    const std::string stopSvcLbl = PmgrStr(l, "stopservice", "Stop service");
    const std::string disableSvcLbl = PmgrStr(l, "disableservice", "Disable startup");

    auto appendExclusion = [&](const std::wstring& exeName) {
        const std::string name = util::ToUtf8(exeName);
        if (settings.processExclusions.find(name) != std::string::npos) return;
        if (!settings.processExclusions.empty()) settings.processExclusions += ", ";
        settings.processExclusions += name;
        settings.Save();
    };

    auto openProcessLocation = [&](const std::wstring& path) {
        if (path.empty()) return;
        wchar_t args[MAX_PATH + 32];
        swprintf_s(args, L"/select,\"%s\"", path.c_str());
        ShellExecuteW(nullptr, nullptr, L"explorer.exe", args, nullptr, SW_SHOWNORMAL);
    };

    if (tab == 0) {
        struct FilteredProc {
            size_t index;
        };
        std::vector<FilteredProc> visible;
        visible.reserve(procs.size());
        for (size_t i = 0; i < procs.size(); ++i) {
            const auto& p = procs[i];
            if (!settings.showSystemProcesses && pmgr::IsSystemProcess(p)) continue;
            if (settings.onlyFrozen && !p.frozen) continue;
            if (pmgr::IsExcluded(p.name, settings.processExclusions)) continue;

            std::wstring searchHay = p.name + L" " + std::to_wstring(p.pid);
            if (!p.path.empty()) searchHay += L" " + p.path;
            if (!pmgr::MatchesSearch(searchHay, searchW)) continue;
            visible.push_back({i});
        }

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable |
            ImGuiTableFlags_SortTristate;
        const int colCount = settings.compact ? 4 : 5;
        const float tableH = ImGui::GetContentRegionAvail().y - 4.0f;

        auto drawProcContextMenu = [&](const pmgr::ProcRow& p, bool allowGroupKill) {
            if (ImGui::MenuItem(endProcLbl.c_str())) {
                if (pmgr::KillProcess(p.pid)) {
                    if (selectedPid == p.pid) selectedPid = 0;
                    needRefresh = true;
                }
            }
            if (allowGroupKill && ImGui::MenuItem(endGroupLbl.c_str())) {
                pmgr::KillProcessesByName(p.name);
                selectedPid = 0;
                needRefresh = true;
            }
            if (ImGui::MenuItem(exclLbl.c_str())) appendExclusion(p.name);
            if (!p.path.empty() && ImGui::MenuItem(openLocLbl.c_str())) openProcessLocation(p.path);
        };

        auto drawProcRow = [&](const pmgr::ProcRow& p, bool allowGroupKill) {
            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(p.pid));
            const bool selected = selectedPid == p.pid;
            ImGui::TableNextColumn();
            const std::string rowLabel = util::ToUtf8(p.name) + "##" + std::to_string(p.pid);
            if (ImGui::Selectable(rowLabel.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                selectedPid = p.pid;
            if (ImGui::BeginPopupContextItem()) {
                drawProcContextMenu(p, allowGroupKill);
                ImGui::EndPopup();
            }
            ImGui::TableNextColumn();
            ImGui::Text("%lu", p.pid);
            ImGui::TableNextColumn();
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

        auto renderProcTable = [&](std::vector<size_t> indices, const bool allowGroupKill) {
            if (indices.empty()) return;
            ImGui::PushID(static_cast<int>(indices.front()));
            if (!ImGui::BeginTable("procs_tbl", colCount, tableFlags, ImVec2(0, tableH))) {
                ImGui::PopID();
                return;
            }
            ImGui::TableSetupColumn(l.Get("pmgr", "main", "process").c_str(),
                                    ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn("PID", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn(l.Get("pmgr", "main", "memuse").c_str(),
                                    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortDescending,
                                    110.0f);
            ImGui::TableSetupColumn(PmgrStr(l, "critical", "Critical").c_str(),
                                    ImGuiTableColumnFlags_WidthFixed, 90.0f);
            if (!settings.compact)
                ImGui::TableSetupColumn(l.Get("pmgr", "main", "location").c_str());
            ImGui::TableHeadersRow();
            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
                if (specs->SpecsDirty) {
                    SortProcIndices(indices, procs, specs);
                    specs->SpecsDirty = false;
                }
            }
            for (const size_t idx : indices) drawProcRow(procs[idx], allowGroupKill);
            ImGui::EndTable();
            ImGui::PopID();
        };

        if (settings.group) {
            std::map<std::wstring, std::vector<size_t>> groups;
            for (const auto& fp : visible) groups[procs[fp.index].name].push_back(fp.index);

            std::vector<std::pair<std::wstring, std::vector<size_t>>> groupList(groups.begin(),
                                                                                groups.end());
            std::sort(groupList.begin(), groupList.end(),
                      [&](const auto& a, const auto& b) {
                          SIZE_T memA = 0;
                          SIZE_T memB = 0;
                          for (size_t i : a.second) memA += procs[i].mem;
                          for (size_t i : b.second) memB += procs[i].mem;
                          return memA > memB;
                      });

            for (auto& [name, indices] : groupList) {
                SIZE_T groupMem = 0;
                for (size_t idx : indices) groupMem += procs[idx].mem;
                const std::string nodeLabel =
                    util::ToUtf8(name) + " (" + std::to_string(indices.size()) + " " +
                    PmgrStr(l, "count", "pcs.") + ", " +
                    std::to_string(static_cast<int>(groupMem / (1024 * 1024))) + " MB)";
                ImGui::PushID(util::ToUtf8(name).c_str());
                if (ImGui::TreeNode(nodeLabel.c_str())) {
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem(endGroupLbl.c_str())) {
                            pmgr::KillProcessesByName(name);
                            selectedPid = 0;
                            needRefresh = true;
                        }
                        ImGui::EndPopup();
                    }
                    renderProcTable(indices, false);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        } else {
            std::vector<size_t> indices;
            indices.reserve(visible.size());
            for (const auto& fp : visible) indices.push_back(fp.index);
            renderProcTable(std::move(indices), true);
        }
    } else {
        struct FilteredSvc {
            size_t index;
        };
        std::vector<FilteredSvc> visible;
        visible.reserve(services.size());
        for (size_t i = 0; i < services.size(); ++i) {
            const auto& s = services[i];
            if (!settings.showSystemProcesses && pmgr::IsSystemService(s)) continue;
            std::wstring searchHay = s.name + L" " + s.displayName;
            if (!pmgr::MatchesSearch(searchHay, searchW)) continue;
            visible.push_back({i});
        }

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Sortable |
            ImGuiTableFlags_SortTristate;
        const float tableH = ImGui::GetContentRegionAvail().y - 4.0f;

        auto drawSvcContextMenu = [&](const pmgr::SvcRow& s) {
            const bool running = s.state == SERVICE_RUNNING;
            if (running && ImGui::MenuItem(stopSvcLbl.c_str())) {
                if (os::EnsureAdmin(nullptr) && pmgr::StopService(s.name))
                    needRefresh = true;
            }
            if (s.startType != SERVICE_DISABLED && ImGui::MenuItem(disableSvcLbl.c_str())) {
                if (os::EnsureAdmin(nullptr) &&
                    pmgr::SetServiceStartType(s.name, SERVICE_DISABLED))
                    needRefresh = true;
            }
        };

        auto drawSvcRow = [&](const pmgr::SvcRow& s) {
            ImGui::TableNextRow();
            ImGui::PushID(util::ToUtf8(s.name).c_str());
            const bool selected = selectedSvc == s.name;
            ImGui::TableNextColumn();
            const std::string rowLabel = util::ToUtf8(s.name) + "##svc";
            if (ImGui::Selectable(rowLabel.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
                selectedSvc = s.name;
            if (ImGui::BeginPopupContextItem()) {
                drawSvcContextMenu(s);
                ImGui::EndPopup();
            }
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

        auto renderSvcTable = [&](std::vector<size_t> indices) {
            if (indices.empty()) return;
            ImGui::PushID(static_cast<int>(indices.front()));
            if (!ImGui::BeginTable("svc_tbl", 5, tableFlags, ImVec2(0, tableH))) {
                ImGui::PopID();
                return;
            }
            ImGui::TableSetupColumn(PmgrStr(l, "servicename", "Service").c_str(),
                                    ImGuiTableColumnFlags_DefaultSort);
            ImGui::TableSetupColumn(PmgrStr(l, "displayname", "Display name").c_str());
            ImGui::TableSetupColumn(PmgrStr(l, "status", "Status").c_str(),
                                    ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupColumn(PmgrStr(l, "starttype", "Startup type").c_str(),
                                    ImGuiTableColumnFlags_WidthFixed, 110.0f);
            ImGui::TableSetupColumn(PmgrStr(l, "critical", "Critical").c_str(),
                                    ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableHeadersRow();
            if (ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs()) {
                if (specs->SpecsDirty) {
                    SortSvcIndices(indices, services, specs);
                    specs->SpecsDirty = false;
                }
            }
            for (const size_t idx : indices) drawSvcRow(services[idx]);
            ImGui::EndTable();
            ImGui::PopID();
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
            std::vector<std::pair<std::wstring, std::vector<size_t>>> groupList(groups.begin(),
                                                                                groups.end());
            std::sort(groupList.begin(), groupList.end(),
                      [&](const auto& a, const auto& b) {
                          return _wcsicmp(a.first.c_str(), b.first.c_str()) < 0;
                      });
            for (auto& [prefix, indices] : groupList) {
                const std::string nodeLabel =
                    util::ToUtf8(prefix) + " (" + std::to_string(indices.size()) + " " +
                    PmgrStr(l, "count", "pcs.") + ")";
                ImGui::PushID(util::ToUtf8(prefix).c_str());
                if (ImGui::TreeNode(nodeLabel.c_str())) {
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem(stopSvcLbl.c_str())) {
                            if (os::EnsureAdmin(nullptr)) {
                                for (size_t idx : indices) pmgr::StopService(services[idx].name);
                                needRefresh = true;
                            }
                        }
                        if (ImGui::MenuItem(disableSvcLbl.c_str())) {
                            if (os::EnsureAdmin(nullptr)) {
                                for (size_t idx : indices)
                                    pmgr::SetServiceStartType(services[idx].name, SERVICE_DISABLED);
                                needRefresh = true;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    renderSvcTable(indices);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        } else {
            std::vector<size_t> indices;
            indices.reserve(visible.size());
            for (const auto& fs : visible) indices.push_back(fs.index);
            renderSvcTable(std::move(indices));
        }
    }
}

// --- PCI ---
static std::string PciLbl(const l10n::Localization& l, const char* key, const char* enFallback) {
    const std::string v = l.Get("pci", "main", key);
    return v == key ? enFallback : v;
}

static std::wstring PciResolveStatus(const l10n::Localization& l, const std::wstring& token) {
    if (token == L"__enabled__") return util::ToWide(PciLbl(l, "st_enabled", "Enabled"));
    if (token == L"__disabled__") return util::ToWide(PciLbl(l, "st_disabled", "Disabled"));
    if (token == L"__enabled_enforced__")
        return util::ToWide(PciLbl(l, "st_enforced", "Enabled (Enforced)"));
    if (token == L"__not_found__")
        return util::ToWide(PciLbl(l, "st_notfound", "Disabled / Not found"));
    if (token == L"__enabled_rt__")
        return util::ToWide(PciLbl(l, "st_defender_rt", "Enabled (Real-Time)"));
    if (token == L"__present__") return util::ToWide(PciLbl(l, "st_tpm_yes", "Present"));
    if (token == L"__absent__") return util::ToWide(PciLbl(l, "st_tpm_no", "Not present"));
    if (token == L"__activated__") return util::ToWide(PciLbl(l, "activated", "Activated"));
    if (token == L"__unlicensed__")
        return util::ToWide(PciLbl(l, "notactivated", "Not activated"));
    if (token == L"__discharging__")
        return util::ToWide(PciLbl(l, "bat_discharging", "Discharging"));
    if (token == L"__ac__") return util::ToWide(PciLbl(l, "bat_ac", "On AC power"));
    if (token == L"__charged__") return util::ToWide(PciLbl(l, "bat_full", "Fully charged"));
    return token;
}

static void DrawAccentValue(const std::wstring& value) {
    auto& app = Application::Instance();
    const bool accent = ui::IsDarkTheme(app.GetSettings().theme);
    if (accent) ImGui::PushStyleColor(ImGuiCol_Text, ui::AccentTextColor());
    if (value.empty())
        ImGui::TextDisabled("—");
    else
        TextWide(value);
    if (accent) ImGui::PopStyleColor();
}

static void DrawInfoRows(
    const std::vector<std::pair<std::string, std::wstring>>& rows,
    const std::function<std::wstring(const std::wstring&)>& mapValue = nullptr) {
    if (rows.empty()) return;
    if (!ImGui::BeginTable("##pci_kv", 2,
                           ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
        return;
    }
    ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthFixed, 240.0f);
    for (const auto& [label, value] : rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(label.c_str());
        ImGui::TableNextColumn();
        const std::wstring shown = mapValue ? mapValue(value) : value;
        if (shown.empty() || shown == L"—")
            ImGui::TextDisabled("—");
        else
            DrawAccentValue(shown);
    }
    ImGui::EndTable();
}

static void DrawInfoCard(const char* id, const std::string& title, const std::wstring& value) {
    ImGui::PushID(id);
    auto& app = Application::Instance();
    const bool dark = ui::IsDarkTheme(app.GetSettings().theme);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::CardSurfaceColor(dark));
    ImGui::BeginChild(id, ImVec2(0, 64), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::TextDisabled("%s", title.c_str());
    DrawAccentValue(value);
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::PopID();
}

static const char* kPciStepKeys[] = {"step_os",     "step_reg",  "step_sec", "step_cpu", "step_ram",
                                      "step_board",  "step_gpu",  "step_disk", "step_net", "step_bat"};
static const char* kPciStepFallbacks[] = {
    "Windows...",           "Regional settings...", "Security...", "Processor...",
    "Memory...",            "Motherboard...",       "Graphics...", "Storage...",
    "Network...",           "Battery..."};

static std::string PciStepLabel(const l10n::Localization& l, int step) {
    if (step < 0) step = 0;
    if (step >= pci::kCollectStepCount) step = pci::kCollectStepCount - 1;
    return PciLbl(l, kPciStepKeys[step], kPciStepFallbacks[step]);
}

static void DrawPci() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    PageTitle(l.Get("pci", "main", "label"));

    auto& collector = pci::PciCollector::Instance();
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) collector.Invalidate();

    if (!collector.IsReady() && !collector.IsRunning()) collector.RequestCollect(false);

    pci::Snapshot snap;
    const bool haveSnap = collector.TryGetSnapshot(snap);
    const bool loading = !haveSnap;
    const auto mapSt = [&](const std::wstring& t) { return PciResolveStatus(l, t); };

    ImGui::BeginChild("pci_scroll", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (ImGui::Button(PciLbl(l, "refresh", "Refresh").c_str())) collector.Invalidate();
    static DWORD pciCopyMsgUntil = 0;
    static std::string pciCopyMsg;
    if (!loading) {
        ImGui::SameLine();
        const auto copyToClipboard = [&](bool full) {
            const report::StatusMapper mapStFn = [&](const std::wstring& t) { return mapSt(t); };
            const std::wstring text =
                full ? report::BuildFullReportFromSnapshot(snap, mapStFn)
                     : report::BuildCompactSummaryFromSnapshot(snap, mapStFn);
            if (clip::SetText(text)) {
                pciCopyMsg = PciLbl(l, "copy_ok", "Copied to clipboard");
                pciCopyMsgUntil = GetTickCount() + 2500;
            } else {
                pciCopyMsg = PciLbl(l, "copy_fail", "Could not copy to clipboard");
                pciCopyMsgUntil = GetTickCount() + 2500;
            }
        };
        if (ImGui::Button(PciLbl(l, "copy_short", "Copy summary").c_str())) copyToClipboard(false);
        ImGui::SameLine();
        if (ImGui::Button(PciLbl(l, "copy_full", "Copy all").c_str())) copyToClipboard(true);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", PciLbl(l, "tooltip", "F5 — refresh").c_str());
    if (pciCopyMsgUntil != 0 && GetTickCount() < pciCopyMsgUntil)
        ImGui::TextDisabled("%s", pciCopyMsg.c_str());

    extras::DrawHwMonitorPanel();
    ImGui::Separator();

    if (loading) {
        ImGui::Spacing();
        const std::string title = PciLbl(l, "loading", "Collecting system information...");
        ImGui::TextUnformatted(title.c_str());
        const int step = collector.CurrentStep();
        const std::string stepText = collector.IsRunning() ? PciStepLabel(l, step) : title;
        const float prog = collector.IsRunning() ? collector.Progress() : 0.f;
        ImGui::ProgressBar(prog, ImVec2(-1.f, 0.f));
        ImGui::TextDisabled("%s", stepText.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("%s",
                           PciLbl(l, "loading_hint",
                                  "You can switch to another tab — collection continues in the "
                                  "background.")
                               .c_str());
        ImGui::EndChild();
        return;
    }

    ImGui::Separator();

    if (app.Launch().pciCompact) {
        std::wstring text = snap.osCaption + L" " + snap.displayVersion + L" (" + snap.build +
                            L")\n" + snap.cpuName;
        if (!snap.gpus.empty()) text += L"\nGPU: " + snap.gpus.front().name;
        text += L"\nRAM: " + pci::FormatBytes(snap.ramTotalBytes);
        ImGui::TextWrapped("%s", util::ToUtf8(text).c_str());
        ImGui::EndChild();
        return;
    }

    if (ImGui::BeginTable("pci_os_cards", 2, ImGuiTableFlags_SizingStretchSame)) {
            auto card = [&](const char* id, const char* key, const std::wstring& val) {
                ImGui::TableNextColumn();
                DrawInfoCard(id, PciLbl(l, key, key), val);
            };
            ImGui::TableNextRow();
            card("os", "oslabel", snap.osCaption);
            card("ver", "verlabel", snap.displayVersion);
            ImGui::TableNextRow();
            card("ed", "editionlabel", snap.edition);
            card("bld", "buildlabel", snap.build);
            ImGui::TableNextRow();
            card("inst", "installdatelabel", snap.installDate);
            card("act", "activationlabel", mapSt(snap.activation));
            ImGui::EndTable();
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "regionsection", "Regional settings").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawInfoRows({{PciLbl(l, "syslanglabel", "System language"), snap.uiLanguage},
                          {PciLbl(l, "regionlabel", "Region"), snap.region},
                          {PciLbl(l, "timezonelabel", "Time zone"), snap.timezone}});
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "virtsection", "Virtualization and isolation").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawInfoRows({{PciLbl(l, "wdaclabel", "App Control (WDAC)"), snap.wdac},
                          {PciLbl(l, "vbslabel", "VBS"), snap.vbs},
                          {PciLbl(l, "hypervlabel", "Hyper-V"), snap.hyperv}},
                         mapSt);
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "secsection", "Protection components").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawInfoRows({{PciLbl(l, "defenderlabel", "Windows Defender"), snap.defender},
                          {PciLbl(l, "coreisollabel", "Core isolation"), snap.coreIsolation},
                          {PciLbl(l, "uaclabel", "UAC"), snap.uac},
                          {PciLbl(l, "smartscreenlabel", "SmartScreen"), snap.smartScreen},
                          {PciLbl(l, "tpmlabel", "TPM"), snap.tpm},
                          {PciLbl(l, "securebootlabel", "Secure Boot"), snap.secureBoot}},
                         mapSt);
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "pcsection", "Computer").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawInfoRows({{PciLbl(l, "hostname", "Computer name"), snap.computerName},
                          {PciLbl(l, "userlabel", "User"), snap.username},
                          {PciLbl(l, "modeln", "Model"),
                           snap.pcManufacturer.empty() ? snap.pcModel
                                                       : snap.pcManufacturer + L" " + snap.pcModel}});
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "processorlabel", "CPU").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            wchar_t cores[64]{};
            swprintf_s(cores, L"%d / %d", snap.cpuCores, snap.cpuThreads);
            wchar_t ghz[32]{};
            swprintf_s(ghz, L"%.2f GHz", snap.cpuBaseGhz);
            wchar_t l3[32]{};
            swprintf_s(l3, L"%d MB", snap.cpuL3Mb);
            DrawInfoRows({{PciLbl(l, "processorname", "Name"), snap.cpuName},
                          {PciLbl(l, "processorcores", "Cores / threads"), std::wstring(cores)},
                          {PciLbl(l, "archlabel", "Architecture"), snap.cpuArch},
                          {PciLbl(l, "processorfreq", "Base frequency"), std::wstring(ghz)},
                          {PciLbl(l, "processorcache", "L3 cache"), std::wstring(l3)}});
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "ramlabel", "RAM").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            wchar_t total[64]{};
            swprintf_s(total, L"%.2f GB", snap.ramTotalBytes / (1024.0 * 1024.0 * 1024.0));
            wchar_t freq[32]{};
            if (snap.ramSpeedMhz > 0) swprintf_s(freq, L"%d MHz", snap.ramSpeedMhz);
            DrawInfoRows(
                {{PciLbl(l, "ramtotal", "Total RAM"), std::wstring(total)},
                 {PciLbl(l, "ramddr", "Type"), snap.ramType},
                 {PciLbl(l, "ramfreq", "Frequency"),
                  snap.ramSpeedMhz > 0 ? std::wstring(freq) : std::wstring(L"—")}});
            if (!snap.ramSticks.empty()) {
                ImGui::Text("%s", PciLbl(l, "ramsticktitle", "RAM modules").c_str());
                for (size_t i = 0; i < snap.ramSticks.size(); ++i) {
                    const auto& m = snap.ramSticks[i];
                    ImGui::Bullet();
                    ImGui::SameLine();
                    wchar_t line[256]{};
                    swprintf_s(line, L"%.2f GB, %d MHz — %s", m.capacityBytes / (1024.0 * 1024.0 * 1024.0),
                               m.speedMhz, m.manufacturer.c_str());
                    TextWide(line);
                    if (!m.partNumber.empty()) {
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s)", util::ToUtf8(m.partNumber).c_str());
                    }
                }
            }
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "mblabel", "Motherboard").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            DrawInfoRows({{PciLbl(l, "mbname", "Name"), snap.motherboard},
                          {PciLbl(l, "mbver", "BIOS version"), snap.biosVersion},
                          {PciLbl(l, "mbdate", "BIOS date"), snap.biosDate}});
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "vlabel", "GPU").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            if (snap.gpus.empty()) {
                ImGui::TextDisabled("—");
            } else {
                for (size_t i = 0; i < snap.gpus.size(); ++i) {
                    const auto& g = snap.gpus[i];
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::TreeNodeEx(util::ToUtf8(g.name).c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        wchar_t vram[64]{};
                        swprintf_s(vram, L"%.2f GB", g.vramBytes / (1024.0 * 1024.0 * 1024.0));
                        DrawInfoRows({{PciLbl(l, "vname", "Name"), g.name},
                                      {PciLbl(l, "vmem", "VRAM"), std::wstring(vram)},
                                      {std::string("Driver"), g.driverVersion}});
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "ssdl", "Storage").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            for (size_t i = 0; i < snap.disks.size(); ++i) {
                const auto& d = snap.disks[i];
                ImGui::PushID(static_cast<int>(i));
                const std::string title = util::ToUtf8(d.model.empty() ? L"Drive" : d.model);
                if (ImGui::TreeNode(title.c_str())) {
                    DrawInfoRows({{PciLbl(l, "sname", "Model"), d.model},
                                  {PciLbl(l, "smem", "Capacity"), pci::FormatBytes(d.sizeBytes)},
                                  {std::string("Interface"), d.interfaceType},
                                  {std::string("Media"), d.mediaType}});
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
            if (!snap.volumes.empty() &&
                ImGui::TreeNode(PciLbl(l, "volsection", "Volumes").c_str())) {
                for (const auto& v : snap.volumes) {
                    ImGui::Bullet();
                    ImGui::SameLine();
                    wchar_t line[256]{};
                    swprintf_s(line, L"%s (%s) — %s, %s %.2f GB", v.id.c_str(), v.fileSystem.c_str(),
                               pci::FormatBytes(v.sizeBytes).c_str(),
                               util::ToWide(PciLbl(l, "volfree", "Free")).c_str(),
                               v.freeBytes / (1024.0 * 1024.0 * 1024.0));
                    TextWide(line);
                }
                ImGui::TreePop();
            }
        }

        if (ImGui::CollapsingHeader(PciLbl(l, "netsection", "Network").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            if (snap.netAdapters.empty()) {
                ImGui::TextDisabled("—");
            } else {
                for (size_t i = 0; i < snap.netAdapters.size(); ++i) {
                    const auto& n = snap.netAdapters[i];
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::TreeNode(util::ToUtf8(n.description).c_str())) {
                        DrawInfoRows({{PciLbl(l, "maclabel", "MAC"), n.mac},
                                      {PciLbl(l, "iplabel", "IPv4"), n.ipv4},
                                      {PciLbl(l, "ipv6label", "IPv6"), n.ipv6}});
                        ImGui::TreePop();
                    }
                    ImGui::PopID();
                }
            }
        }

        if (snap.hasBattery &&
            ImGui::CollapsingHeader(PciLbl(l, "batterysection", "Battery").c_str(),
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
            wchar_t ch[32]{};
            if (snap.battery.chargePercent >= 0)
                swprintf_s(ch, L"%d%%", snap.battery.chargePercent);
            DrawInfoRows(
                {{PciLbl(l, "chargelabel", "Charge"),
                  snap.battery.chargePercent >= 0 ? std::wstring(ch) : std::wstring(L"—")},
                 {PciLbl(l, "batterystatus", "Status"), snap.battery.status}},
                mapSt);
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

    if (ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::IsKeyDown(ImGuiKey_LeftCtrl)) {
        std::wstring path = util::GetExeDirectory() + L"\\pci_export.txt";
        std::wofstream out(path);
        out << L"OS: " << snap.osCaption << L"\nBuild: " << snap.build
            << L"\nCPU: " << snap.cpuName << L"\nRAM: " << pci::FormatBytes(snap.ramTotalBytes);
    }

    ImGui::EndChild();
}

static void DrawSettings() {
    auto& app = Application::Instance();
    auto& l = app.L10n();
    auto& s = app.GetSettings();
    PageTitle(l.Get("base", "lowtabs", "set"));

    const int langIdx = l10n::LanguageIndex(s.lang);

    ImGui::TextUnformatted(l.Get("ab", "main", "lang").c_str());
    ImGui::SameLine();
    if (ImGui::BeginCombo("##lang", l10n::kLanguages[langIdx].nativeName)) {
        for (int i = 0; i < l10n::kLanguageCount; ++i) {
            const bool selected = (langIdx == i);
            if (ImGui::Selectable(l10n::kLanguages[i].nativeName, selected)) {
                s.lang = l10n::kLanguages[i].tag;
                s.Save();
                app.RequestLanguageReload();
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::TextUnformatted(l.Get("ab", "main", "th").c_str());
    const char* themes[] = {"Light", "Dark"};
    const char* themeKeys[] = {"l", "d"};
    for (int i = 0; i < 2; ++i) {
        if (i > 0) ImGui::SameLine();
        const bool sel = s.theme == themes[i];
        if (ImGui::RadioButton(l.Get("ab", "main", themeKeys[i]).c_str(), sel)) {
            if (!sel) {
                s.theme = ui::NormalizeTheme(themes[i]);
                s.Save();
                ui::ApplyTheme(s.theme);
                app.RefreshGlassPipeline();
            }
        }
    }

    bool topmost = s.topmost;
    if (ImGui::Checkbox(l.Get("ab", "main", "topmost").c_str(), &topmost)) app.SetTopmost(topmost);

    ImGui::Separator();
    ImGui::Text("MakuTweaker++ 5.6 (C++/ImGui)");
    ImGui::Text("Mark Adderly");
    if (ImGui::Button(l.Get("ab", "main", "github").c_str()))
        proc::OpenUrl(L"https://github.com/MarkAdderly/MakuTweaker");
    if (ImGui::Button(l.Get("ab", "main", "donate").c_str())) proc::OpenUrl(L"https://adderly.top/mt");

    ImGui::Separator();
    bool metricsSvc = s.metricsServiceEnabled;
    if (ImGui::Checkbox(l.Get("ab", "main", "metrics_service").c_str(), &metricsSvc)) {
        s.metricsServiceEnabled = metricsSvc;
        s.Save();
        if (metricsSvc) {
            metrics_svc::EnsureAutostartAndRunning();
        } else {
            metrics_svc::Stop();
        }
    }
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::InputInt(l.Get("ab", "main", "metrics_interval").c_str(), &s.metricsIntervalSec)) {
        s.metricsIntervalSec = std::clamp(s.metricsIntervalSec, 1, 60);
        s.Save();
    }

    ImGui::Separator();
    static char exclusions[1024]{};
    if (exclusions[0] == 0 && !s.processExclusions.empty())
        strncpy_s(exclusions, s.processExclusions.c_str(), _TRUNCATE);
    ImGui::InputTextMultiline(l.Get("pmgr", "main", "exclinfo").c_str(), exclusions,
                              sizeof(exclusions), ImVec2(-1, 60));
    if (ImGui::Button(l.Get("pmgr", "main", "save").c_str())) {
        s.processExclusions = exclusions;
        s.Save();
    }

    ImGui::Separator();
    if (logf::Active()) {
        ImGui::TextDisabled("%s", util::ToUtf8(logf::Path()).c_str());
        if (ImGui::Button(l.Def("open_log").c_str())) {
            const std::wstring folder =
                std::filesystem::path(logf::Path()).parent_path().wstring();
            proc::Run(L"explorer.exe", L"\"" + folder + L"\"");
        }
    }

    ImGui::Separator();
    if (ImGui::Button(l.Get("ab", "main", "export_mktw").c_str())) {
        nlohmann::json j;
        j["lang"] = s.lang;
        j["theme"] = s.theme;
        j["processExclusions"] = s.processExclusions;
        j["makuYanPar"] = s.makuYanPar;
        j["autoTtlOnBoot"] = s.autoTtlOnBoot;
        j["autoWeeklyCleanup"] = s.autoWeeklyCleanup;
        j["processWatchdog"] = s.processWatchdog;
        std::ofstream out(util::ToUtf8(util::GetExeDirectory() + L"\\export.mktw"));
        if (out) out << j.dump(2);
    }
    ImGui::SameLine();
    if (ImGui::Button(l.Get("ab", "main", "import_mktw").c_str())) {
        preset::ApplyFile(util::GetExeDirectory() + L"\\export.mktw", s, false);
        app.RequestLanguageReload();
    }

    static bool showMakuYan{};
    if (ImGui::Button("MakuYan")) showMakuYan = true;
    if (showMakuYan) {
        ImGui::OpenPopup("MakuYan");
        if (ImGui::BeginPopupModal("MakuYan", &showMakuYan, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("MakuYan says: tweak responsibly!");
            if (ImGui::Button("OK")) showMakuYan = false;
            ImGui::EndPopup();
        }
    }
}

} // namespace

void Draw(app::PageId page) {
    if (jobs::JobQueue::Instance().IsBusy() && !g_uwpRemovalRunning.load()) {
        float pv = jobs::JobQueue::Instance().ProgressValue();
        if (pv < 0.f) pv = 0.5f;
        ImGui::Text("Working: %ls", jobs::JobQueue::Instance().ProgressText().c_str());
        ImGui::ProgressBar(pv, ImVec2(-1, 0));
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
    case app::PageId::Monitor: DrawMonitor(); break;
    case app::PageId::Settings: DrawSettings(); break;
    default: break;
    }
}

} // namespace maku::ui::pages
