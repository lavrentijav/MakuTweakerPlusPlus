#include "ui/Modals.h"
#include "app/Application.h"
#include "app/Branding.h"
#include "core/Analytics.h"
#include "core/DisallowRun.h"
#include "core/HostsBan.h"
#include "core/StringUtil.h"
#include "core/TaskMgrReplace.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace maku::ui::modals {
namespace {

static bool g_siteBan{};
static bool g_makuYan{};
static bool g_exclusion{};
static bool g_consent{};

static void ShowError(const std::wstring& msg) {
    MessageBoxW(maku::app::Application::Instance().Hwnd(), msg.c_str(), brand::kDisplayName,
                MB_OK | MB_ICONERROR);
}

} // namespace

void OpenSiteBan() { g_siteBan = true; }
void OpenMakuYan() { g_makuYan = true; }
void OpenExclusionSettings() { g_exclusion = true; }
void OpenAnalyticsConsent() { g_consent = true; }

void DrawAll() {
    auto& app = maku::app::Application::Instance();
    auto& l = app.L10n();
    auto& s = app.GetSettings();

    if (g_consent) {
        ImGui::OpenPopup("AnalyticsConsent");
        // No close button and no click-outside dismissal: leaving the question
        // unanswered would mean guessing, and the safe guess (off) should be an
        // explicit choice rather than an accident.
        ImGui::SetNextWindowSize(ImVec2(520.f * UiScale(), 0.f), ImGuiCond_Appearing);
        const ImVec2 viewport = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(viewport.x * 0.5f, viewport.y * 0.5f), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("AnalyticsConsent", nullptr,
                                   ImGuiWindowFlags_AlwaysAutoResize |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoTitleBar)) {
            ImGui::PushFont(FontTitle());
            ImGui::TextUnformatted(l.Get("ab", "consent", "title").c_str());
            ImGui::PopFont();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", l.Get("ab", "consent", "body").c_str());
            ImGui::Spacing();
            ImGui::BulletText("%s", l.Get("ab", "consent", "item_cpu").c_str());
            ImGui::BulletText("%s", l.Get("ab", "consent", "item_tabs").c_str());
            ImGui::Spacing();
            ImGui::TextWrapped("%s", l.Get("ab", "consent", "note").c_str());
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            const float buttonW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
            if (ImGui::Button(l.Get("ab", "consent", "allow").c_str(), ImVec2(buttonW, 0.f))) {
                analytics::SetConsent(AnalyticsConsent::Granted);
                s.analyticsConsent = static_cast<int>(AnalyticsConsent::Granted);
                s.disableTelemetry = false;
                analytics::ScheduleLaunchEvents(s.lang);
                g_consent = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(l.Get("ab", "consent", "decline").c_str(), ImVec2(buttonW, 0.f))) {
                analytics::SetConsent(AnalyticsConsent::Declined);
                s.analyticsConsent = static_cast<int>(AnalyticsConsent::Declined);
                s.disableTelemetry = true;
                g_consent = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    if (g_siteBan) {
        ImGui::OpenPopup("SiteBan");
        static char domainsBuf[8192]{};
        static bool loaded{};
        if (!loaded) {
            auto domains = hosts::ReadBlockedDomains();
            std::string text;
            for (size_t i = 0; i < domains.size(); ++i) {
                if (i) text += "\n";
                text += domains[i];
            }
            strncpy_s(domainsBuf, text.c_str(), _TRUNCATE);
            loaded = true;
        }
        if (ImGui::BeginPopupModal("SiteBan", &g_siteBan,
                                   ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", l.Get("myan", "main", "siteban").c_str());
            ImGui::InputTextMultiline("##domains", domainsBuf, sizeof(domainsBuf), ImVec2(500, 200));
            const char* yandexLangs[] = {"ru", "uk", "kk", "lv", "et", "be", "az"};
            bool showYandex = false;
            for (auto tag : yandexLangs)
                if (s.lang == tag) showYandex = true;
            if (showYandex && ImGui::Button(l.Get("myan", "main", "banyandex").c_str())) {
                auto preset = hosts::YandexPreset();
                std::string text;
                for (size_t i = 0; i < preset.size(); ++i) {
                    if (i) text += "\n";
                    text += preset[i];
                }
                strncpy_s(domainsBuf, text.c_str(), _TRUNCATE);
            }
            ImGui::SameLine();
            if (ImGui::Button(l.Get("myan", "main", "clear").c_str())) domainsBuf[0] = 0;
            if (ImGui::Button(l.Get("myan", "main", "applyban").c_str())) {
                const auto domains = hosts::ParseDomainsFromText(domainsBuf);
                if (hosts::ContainsForbiddenDomain(domains)) {
                    ShowError(util::ToWide(l.Get("myan", "main", "makutnah")));
                } else {
                    std::wstring err;
                    if (hosts::UpdateBlockedDomains(domains, &err)) {
                        MessageBoxW(app.Hwnd(),
                                    util::ToWide(l.Get("myan", "main", "sitebandone")).c_str(),
                                    brand::kDisplayName, MB_OK | MB_ICONINFORMATION);
                        g_siteBan = false;
                        loaded = false;
                    } else {
                        ShowError(err.empty() ? L"Failed to update hosts file (admin required?)"
                                              : err);
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(l.Def("off").c_str())) g_siteBan = false;
            ImGui::EndPopup();
        }
    }

    if (g_makuYan) {
        ImGui::OpenPopup("MakuYan");
        static char blockBuf[4096]{};
        if (blockBuf[0] == 0 && !s.makuYanPar.empty()) strncpy_s(blockBuf, s.makuYanPar.c_str(), _TRUNCATE);
        if (ImGui::BeginPopupModal("MakuYan", &g_makuYan, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", l.Get("myan", "main", "info").c_str());
            ImGui::InputTextMultiline("##makuyan", blockBuf, sizeof(blockBuf), ImVec2(480, 120));
            if (ImGui::Button(l.Get("myan", "main", "applyban").c_str())) {
                std::string text;
                for (const char* p = blockBuf; *p; ++p) {
                    char c = static_cast<char>(tolower(static_cast<unsigned char>(*p)));
                    if (c != ' ') text += c;
                }
                if (disallow::ContainsForbiddenProcess(text)) {
                    ShowError(util::ToWide(l.Get("myan", "main", "makutnah")));
                } else {
                    std::wstring err;
                    if (disallow::ApplyProcessBlockList(text, &err)) {
                        s.makuYanPar = text;
                        s.Save();
                        app.NotifyReboot(1);
                        g_makuYan = false;
                    } else
                        ShowError(err);
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(l.Get("myan", "main", "clear").c_str())) blockBuf[0] = 0;
            ImGui::EndPopup();
        }
    }

    if (g_exclusion) {
        ImGui::OpenPopup("ExclusionSettings");
        static char exclBuf[1024]{};
        if (exclBuf[0] == 0 && !s.processExclusions.empty())
            strncpy_s(exclBuf, s.processExclusions.c_str(), _TRUNCATE);
        if (ImGui::BeginPopupModal("ExclusionSettings", &g_exclusion, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", l.Get("pmgr", "main", "exclinfo").c_str());
            ImGui::InputTextMultiline("##excl", exclBuf, sizeof(exclBuf), ImVec2(460, 80));
            ImGui::Checkbox(l.Get("pmgr", "main", "onlymb").c_str(), &s.ramMbOnly);
            ImGui::Checkbox(l.Get("pmgr", "main", "showsysproc").c_str(), &s.showSystemProcs);
            if (ImGui::Checkbox(l.Get("pmgr", "main", "taskmgr").c_str(), &s.replaceTaskMgr)) {
                std::wstring err;
                if (s.replaceTaskMgr) {
                    if (!taskmgr::SetReplace(true, &err)) ShowError(err);
                } else
                    taskmgr::SetReplace(false, nullptr);
            }
            const char* modes[] = {l.Get("pmgr", "main", "friendly").c_str(),
                                   l.Get("pmgr", "main", "systematic").c_str(),
                                   l.Get("pmgr", "main", "bothvar").c_str()};
            ImGui::Combo(l.Get("pmgr", "main", "viewtype").c_str(), &s.processViewMode, modes, 3);
            ImGui::SliderInt(l.Get("pmgr", "main", "updspeed").c_str(), &s.monitoringRefreshMs, 250,
                             3000);
            s.cpuAutoTopK = std::clamp(s.cpuAutoTopK, 3, 16);
            ImGui::SliderInt(l.Get("pmgr", "main", "cpu_topk").c_str(), &s.cpuAutoTopK, 3, 16);
            if (ImGui::Button(l.Get("pmgr", "main", "save").c_str())) {
                s.processExclusions = exclBuf;
                s.Save();
                g_exclusion = false;
            }
            ImGui::SameLine();
            if (ImGui::Button(l.Def("off").c_str())) g_exclusion = false;
            ImGui::EndPopup();
        }
    }
}

} // namespace maku::ui::modals
