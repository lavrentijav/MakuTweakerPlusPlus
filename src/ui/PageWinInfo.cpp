#include "ui/Pages.h"
#include "ui/UiCommon.h"
#include "ui/Theme.h"
#include "app/Application.h"
#include "core/Localization.h"
#include "core/OsUtil.h"
#include "core/Registry.h"
#include "core/StringUtil.h"
#include "core/SysInfoFormat.h"
#include "core/Wmi.h"
#include <imgui.h>
#include <psapi.h>
#include <ctime>
#include <algorithm>
#include <cstring>
#include <tlhelp32.h>

namespace maku::ui::pages {
namespace {

using app::Application;

struct WinInfoData {
    bool loaded{};
    std::string osName, version, edition, build, installDate, activation;
    std::string language, region, timezone;
    std::string coreIsol, uac, appControl, vbs, hyperv, defender, smartScreen;
};

WinInfoData& Data() {
    static WinInfoData d;
    return d;
}

std::string ReadInstallDate() {
    DWORD installUnix = 0;
    if (reg::GetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                      L"InstallDate", installUnix) &&
        installUnix > 0) {
        const std::time_t t = static_cast<std::time_t>(installUnix);
        std::tm tm{};
        if (localtime_s(&tm, &t) == 0) {
            char buf[32]{};
            snprintf(buf, sizeof(buf), "%02d.%02d.%04d", tm.tm_mday, tm.tm_mon + 1,
                     tm.tm_year + 1900);
            return buf;
        }
    }
    return sysfmt::FormatCimDate(util::ToUtf8(wmi::QueryScalar(
        L"SELECT InstallDate FROM Win32_OperatingSystem", L"InstallDate")));
}

void LoadWinInfo(const l10n::Localization& l) {
    auto& d = Data();
    if (d.loaded) return;

    d.osName = util::ToUtf8(wmi::QueryScalar(L"SELECT Caption FROM Win32_OperatingSystem", L"Caption"));
    d.installDate = ReadInstallDate();
    std::wstring dv, rid;
    reg::GetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                   L"DisplayVersion", dv);
    reg::GetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                   L"ReleaseId", rid);
    d.version = util::ToUtf8(dv.empty() ? rid : dv);
    std::wstring editionW;
    reg::GetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                   L"EditionID", editionW);
    d.edition = util::ToUtf8(editionW);
    std::wstring currentBuild;
    reg::GetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                   L"CurrentBuild", currentBuild);
    if (currentBuild.empty())
        reg::GetString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
                       L"CurrentBuildNumber", currentBuild);
    DWORD ubr = 0;
    reg::GetDword(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"UBR",
                  ubr);
    d.build = util::ToUtf8(currentBuild) + "." + std::to_string(ubr);

    const auto lic = wmi::QueryScalar(
        L"SELECT LicenseStatus FROM SoftwareLicensingProduct WHERE PartialProductKey IS NOT NULL",
        L"LicenseStatus");
    d.activation = (lic == L"1") ? l.Get("wininfo", "main", "activated")
                                 : l.Get("wininfo", "main", "not_activated");

    wchar_t locale[128]{};
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SENGLANGUAGE, locale, 128);
    d.language = util::ToUtf8(locale);

    wchar_t country[128]{};
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SENGCOUNTRY, country, 128);
    d.region = util::ToUtf8(country);

    DYNAMIC_TIME_ZONE_INFORMATION tz{};
    if (GetDynamicTimeZoneInformation(&tz) == TIME_ZONE_ID_INVALID)
        d.timezone = l.Get("wininfo", "main", "unknown");
    else
        d.timezone = util::ToUtf8(tz.TimeZoneKeyName);

    DWORD hvci = 0;
    reg::GetDword(HKEY_LOCAL_MACHINE,
                  L"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\"
                  L"HypervisorEnforcedCodeIntegrity",
                  L"Enabled", hvci);
    d.coreIsol = hvci ? l.Get("wininfo", "main", "status_on")
                      : l.Get("wininfo", "main", "status_off");

    DWORD enableLua = 1, consent = 5;
    reg::GetDword(HKEY_LOCAL_MACHINE,
                  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", L"EnableLUA",
                  enableLua);
    reg::GetDword(HKEY_LOCAL_MACHINE,
                  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                  L"ConsentPromptBehaviorAdmin", consent);
    if (!enableLua)
        d.uac = l.Get("wininfo", "main", "status_off");
    else if (!consent)
        d.uac = l.Get("wininfo", "main", "uac_off_no_prompt");
    else
        d.uac = l.Get("wininfo", "main", "status_on");

    d.appControl = l.Get("wininfo", "main", "unknown");
    const std::wstring vbsW = wmi::QueryScalar(
        L"SELECT VirtualizationBasedSecurityStatus FROM Win32_DeviceGuard",
        L"VirtualizationBasedSecurityStatus");
    if (vbsW == L"2")
        d.vbs = l.Get("wininfo", "main", "working");
    else if (vbsW == L"1")
        d.vbs = l.Get("wininfo", "main", "on_not_working");
    else if (vbsW.empty())
        d.vbs = l.Get("wininfo", "main", "not_supported");
    else
        d.vbs = l.Get("wininfo", "main", "status_off");

    d.hyperv = wmi::QueryScalar(L"SELECT HypervisorPresent FROM Win32_ComputerSystem",
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
    d.defender = defenderProc && !disableRt ? l.Get("wininfo", "main", "on_realtime")
                                            : l.Get("wininfo", "main", "status_off");

    std::wstring ss;
    reg::GetString(HKEY_LOCAL_MACHINE,
                   L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer", L"SmartScreenEnabled",
                   ss);
    d.smartScreen = (_wcsicmp(ss.c_str(), L"Off") == 0) ? l.Get("wininfo", "main", "status_off")
                                                        : l.Get("wininfo", "main", "status_on");

    d.loaded = true;
}

enum class WinInfoIcon { Os, Version, Edition, Build, Calendar, Shield };

void DrawWinInfoIcon(ImDrawList* dl, ImVec2 center, WinInfoIcon kind, ImU32 col) {
    const float s = 7.f;
    switch (kind) {
    case WinInfoIcon::Os: {
        dl->AddRect(ImVec2(center.x - s, center.y - s * 0.75f), ImVec2(center.x + s, center.y + s),
                    col, 2.f, 0, 1.4f);
        dl->AddLine(ImVec2(center.x - s, center.y - s * 0.2f), ImVec2(center.x + s, center.y - s * 0.2f),
                    col, 1.2f);
        break;
    }
    case WinInfoIcon::Version:
        dl->AddLine(ImVec2(center.x - s, center.y + s * 0.5f), ImVec2(center.x + s, center.y - s * 0.6f),
                    col, 1.4f);
        dl->AddCircleFilled(ImVec2(center.x - s * 0.55f, center.y + s * 0.35f), 2.f, col);
        dl->AddCircleFilled(ImVec2(center.x + s * 0.55f, center.y - s * 0.45f), 2.f, col);
        break;
    case WinInfoIcon::Edition:
        dl->AddRect(ImVec2(center.x - s, center.y - s), ImVec2(center.x, center.y), col, 1.f, 0, 1.2f);
        dl->AddRect(ImVec2(center.x, center.y), ImVec2(center.x + s, center.y + s), col, 1.f, 0, 1.2f);
        break;
    case WinInfoIcon::Build:
        dl->AddLine(ImVec2(center.x - s * 0.2f, center.y - s), ImVec2(center.x - s * 0.2f, center.y + s),
                    col, 1.4f);
        dl->AddLine(ImVec2(center.x - s * 0.2f, center.y + s * 0.35f),
                    ImVec2(center.x + s * 0.75f, center.y - s * 0.15f), col, 1.4f);
        break;
    case WinInfoIcon::Calendar:
        dl->AddRect(ImVec2(center.x - s, center.y - s * 0.55f), ImVec2(center.x + s, center.y + s),
                    col, 2.f, 0, 1.3f);
        dl->AddLine(ImVec2(center.x - s, center.y - s * 0.1f), ImVec2(center.x + s, center.y - s * 0.1f),
                    col, 1.2f);
        break;
    case WinInfoIcon::Shield:
        dl->AddTriangleFilled(ImVec2(center.x, center.y - s),
                              ImVec2(center.x - s * 0.85f, center.y - s * 0.15f),
                              ImVec2(center.x + s * 0.85f, center.y - s * 0.15f), col);
        dl->AddRectFilled(ImVec2(center.x - s * 0.75f, center.y - s * 0.1f),
                          ImVec2(center.x + s * 0.75f, center.y + s * 0.85f), col, 2.f);
        break;
    }
}

void DrawInfoCard(const char* id, const char* label, const char* value, WinInfoIcon icon, bool dark) {
    const float scale = UiScale();
    const float cardH = 76.f * scale;
    const float cardW = ImGui::GetContentRegionAvail().x;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 cardCol = ImGui::ColorConvertFloat4ToU32(CardSurfaceColor(dark));
    const ImU32 iconCol = AccentBarU32();
    const ImVec2 p1(p0.x + cardW, p0.y + cardH);
    dl->AddRectFilled(p0, p1, cardCol, CardRounding());

    DrawWinInfoIcon(dl, ImVec2(p0.x + 22.f * scale, p0.y + cardH * 0.5f), icon, iconCol);

    const ImU32 labelCol = IM_COL32(200, 195, 185, 255);
    const ImVec4 accent = AccentTextColor();
    const ImU32 valueCol = ImGui::ColorConvertFloat4ToU32(accent);

    dl->AddText(ImVec2(p0.x + 44.f * scale, p0.y + 14.f * scale), labelCol, label);
    const char* valueLine = value;
    const ImVec2 valSize = ImGui::CalcTextSize(valueLine);
    const float maxValW = cardW - 48.f * scale;
    if (valSize.x > maxValW && valueLine[0]) {
        ImGui::PushClipRect(ImVec2(p0.x + 44.f * scale, p0.y),
                            ImVec2(p0.x + cardW - 6.f * scale, p0.y + cardH), true);
    }
    dl->AddText(ImVec2(p0.x + 44.f * scale, p0.y + 36.f * scale), valueCol, valueLine);
    if (valSize.x > maxValW) ImGui::PopClipRect();

    ImGui::InvisibleButton(id, ImVec2(cardW, cardH));
}

} // namespace

void DrawWinInfo() {
    auto& l = Application::Instance().L10n();
    const bool dark = IsDarkTheme(Application::Instance().GetSettings().theme);
    PageTitle(l.CatName("wininfo"));
    LoadWinInfo(l);
    const auto& d = Data();

    const float gap = 8.f * UiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(gap * 0.5f, gap * 0.5f));
    if (ImGui::BeginTable("wininfo_grid", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawInfoCard("card_os", l.Get("wininfo", "main", "name").c_str(), d.osName.c_str(),
                     WinInfoIcon::Os, dark);
        ImGui::TableSetColumnIndex(1);
        DrawInfoCard("card_ver", l.Get("wininfo", "main", "version").c_str(), d.version.c_str(),
                     WinInfoIcon::Version, dark);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawInfoCard("card_ed", l.Get("wininfo", "main", "edition").c_str(), d.edition.c_str(),
                     WinInfoIcon::Edition, dark);
        ImGui::TableSetColumnIndex(1);
        DrawInfoCard("card_build", l.Get("wininfo", "main", "build").c_str(), d.build.c_str(),
                     WinInfoIcon::Build, dark);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        DrawInfoCard("card_date", l.Get("wininfo", "main", "install_date").c_str(),
                     d.installDate.c_str(), WinInfoIcon::Calendar, dark);
        ImGui::TableSetColumnIndex(1);
        DrawInfoCard("card_act", l.Get("wininfo", "main", "activation").c_str(),
                     d.activation.c_str(), WinInfoIcon::Shield, dark);

        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::Dummy(ImVec2(0.f, 4.f * UiScale()));

    if (BeginCollapsibleSection("locale", l.Get("wininfo", "main", "locale_header").c_str(), dark, true,
                                MetricCardIcon::Generic)) {
        DrawSettingsCard("wi_lang", l.Get("wininfo", "main", "language").c_str(), d.language.c_str(),
                         MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_reg", l.Get("wininfo", "main", "region").c_str(), d.region.c_str(),
                         MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_tz", l.Get("wininfo", "main", "timezone").c_str(), d.timezone.c_str(),
                         MetricCardIcon::Generic, dark);
        EndCollapsibleSection();
    }

    if (BeginCollapsibleSection("isol", l.Get("wininfo", "main", "isolation_header").c_str(), dark,
                                false, MetricCardIcon::Board)) {
        DrawSettingsCard("wi_ci", l.Get("wininfo", "main", "coreisol").c_str(), d.coreIsol.c_str(),
                         MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_ac", "App Control", d.appControl.c_str(), MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_vbs", "VBS", d.vbs.c_str(), MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_hv", "Hyper-V", d.hyperv.c_str(), MetricCardIcon::Generic, dark);
        EndCollapsibleSection();
    }

    if (BeginCollapsibleSection("prot", l.Get("wininfo", "main", "protection_header").c_str(), dark,
                                false, MetricCardIcon::Generic)) {
        DrawSettingsCard("wi_def", "Defender", d.defender.c_str(), MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_uac", l.Get("wininfo", "main", "uac").c_str(), d.uac.c_str(),
                         MetricCardIcon::Generic, dark);
        DrawSettingsCard("wi_ss", "SmartScreen", d.smartScreen.c_str(), MetricCardIcon::Generic,
                         dark);
        EndCollapsibleSection();
    }
}

} // namespace maku::ui::pages
