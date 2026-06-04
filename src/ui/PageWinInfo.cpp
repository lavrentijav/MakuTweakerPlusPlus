#include "ui/Pages.h"
#include "ui/UiCommon.h"
#include "app/Application.h"
#include "core/OsUtil.h"
#include "core/Registry.h"
#include "core/StringUtil.h"
#include "core/Wmi.h"
#include <imgui.h>
#include <psapi.h>
#include <tlhelp32.h>

namespace maku::ui::pages {

void DrawWinInfo() {
    auto& l = maku::app::Application::Instance().L10n();
    PageTitle(l.CatName("wininfo"));

    static bool loaded{};
    static std::string osName, version, edition, build, installDate, activation;
    static std::string language, region, timezone;
    static std::string coreIsol, uac, appControl, vbs, hyperv, defender, smartScreen;

    if (!loaded) {
        osName = util::ToUtf8(wmi::QueryScalar(L"SELECT Caption FROM Win32_OperatingSystem", L"Caption"));
        installDate = util::ToUtf8(
            wmi::QueryScalar(L"SELECT InstallDate FROM Win32_OperatingSystem", L"InstallDate"));
        std::wstring dv, rid;
        reg::GetString(HKEY_LOCAL_MACHINE,
                       L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion", dv);
        reg::GetString(HKEY_LOCAL_MACHINE,
                       L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ReleaseId", rid);
        version = util::ToUtf8(dv.empty() ? rid : dv);
        std::wstring editionW;
        reg::GetString(HKEY_LOCAL_MACHINE,
                       L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"EditionID", editionW);
        edition = util::ToUtf8(editionW);
        DWORD cb = 0, ubr = 0;
        reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"CurrentBuild", cb);
        reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR", ubr);
        build = std::to_string(cb) + "." + std::to_string(ubr);

        const auto lic = wmi::QueryScalar(
            L"SELECT LicenseStatus FROM SoftwareLicensingProduct WHERE PartialProductKey IS NOT NULL",
            L"LicenseStatus");
        activation = (lic == L"1") ? l.Get("wininfo", "main", "activated")
                                   : l.Get("wininfo", "main", "not_activated");

        wchar_t locale[128]{};
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, locale, 128);
        language = util::ToUtf8(locale);

        wchar_t country[128]{};
        GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SENGCOUNTRY, country, 128);
        region = util::ToUtf8(country);

        DYNAMIC_TIME_ZONE_INFORMATION tz{};
        if (GetDynamicTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID)
            timezone = l.Get("wininfo", "main", "unknown");
        else
            timezone = util::ToUtf8(tz.TimeZoneKeyName);

        DWORD hvci = 0;
        reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
                      L"Enabled", hvci);
        coreIsol = hvci ? l.Get("wininfo", "main", "status_on")
                        : l.Get("wininfo", "main", "status_off");

        DWORD enableLua = 1, consent = 5;
        reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA",
                      enableLua);
        reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                      L"ConsentPromptBehaviorAdmin", consent);
        if (!enableLua)
            uac = l.Get("wininfo", "main", "status_off");
        else if (!consent)
            uac = l.Get("wininfo", "main", "uac_off_no_prompt");
        else
            uac = l.Get("wininfo", "main", "status_on");

        appControl = l.Get("wininfo", "main", "unknown");
        const std::wstring vbsW = wmi::QueryScalar(
            L"SELECT VirtualizationBasedSecurityStatus FROM Win32_DeviceGuard",
            L"VirtualizationBasedSecurityStatus");
        if (vbsW == L"2") vbs = l.Get("wininfo", "main", "working");
        else if (vbsW == L"1") vbs = l.Get("wininfo", "main", "on_not_working");
        else if (vbsW.empty()) vbs = l.Get("wininfo", "main", "not_supported");
        else vbs = l.Get("wininfo", "main", "status_off");

        hyperv = wmi::QueryScalar(L"SELECT HypervisorPresent FROM Win32_ComputerSystem",
                                  L"HypervisorPresent") == L"TRUE"
                     ? l.Get("wininfo", "main", "present")
                     : l.Get("wininfo", "main", "status_off_or_not_found");

        bool defenderProc = false;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W pe{};
            pe.dwSize = sizeof(pe);
            if (Process32FirstW(snap, &pe)) {
                do {
                    if (_wcsicmp(pe.szExeFile, L"MsMpEng.exe") == 0) {
                        defenderProc = true;
                        break;
                    }
                } while (Process32NextW(snap, &pe));
            }
            CloseHandle(snap);
        }
        DWORD disableRt = 0;
        reg::GetDword(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\Microsoft\\Windows Defender\\Real-Time Protection",
                      L"DisableRealtimeMonitoring", disableRt);
        defender = defenderProc && !disableRt ? l.Get("wininfo", "main", "on_realtime")
                                              : l.Get("wininfo", "main", "status_off");

        std::wstring ss;
        reg::GetString(HKEY_LOCAL_MACHINE,
                       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"SmartScreenEnabled",
                       ss);
        smartScreen = (_wcsicmp(ss.c_str(), L"Off") == 0)
                          ? l.Get("wininfo", "main", "status_off")
                          : l.Get("wininfo", "main", "status_on");

        loaded = true;
    }

    auto card = [&](const char* title, const char* value) {
        ImGui::SeparatorText(title);
        ImGui::TextWrapped("%s", value);
    };

    card(l.Get("wininfo", "main", "name").c_str(), osName.c_str());
    card(l.Get("wininfo", "main", "version").c_str(), version.c_str());
    card(l.Get("wininfo", "main", "edition").c_str(), edition.c_str());
    card(l.Get("wininfo", "main", "build").c_str(), build.c_str());
    card(l.Get("wininfo", "main", "install_date").c_str(), installDate.c_str());
    card(l.Get("wininfo", "main", "activation").c_str(), activation.c_str());

    ImGui::SeparatorText(l.Get("wininfo", "main", "locale_header").c_str());
    card(l.Get("wininfo", "main", "language").c_str(), language.c_str());
    card(l.Get("wininfo", "main", "region").c_str(), region.c_str());
    card(l.Get("wininfo", "main", "timezone").c_str(), timezone.c_str());

    ImGui::SeparatorText(l.Get("wininfo", "main", "isolation_header").c_str());
    card(l.Get("wininfo", "main", "coreisol").c_str(), coreIsol.c_str());
    card("App Control", appControl.c_str());
    card("VBS", vbs.c_str());
    card("Hyper-V", hyperv.c_str());

    ImGui::SeparatorText(l.Get("wininfo", "main", "protection_header").c_str());
    card("Defender", defender.c_str());
    card(l.Get("wininfo", "main", "uac").c_str(), uac.c_str());
    card("SmartScreen", smartScreen.c_str());
}

} // namespace maku::ui::pages
