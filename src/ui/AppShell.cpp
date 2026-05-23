#include "ui/AppShell.h"

#include "app/Branding.h"
#include "ui/Pages.h"
#include "ui/Search.h"
#include "ui/Theme.h"
#include "ui/Fonts.h"
#include "app/Application.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <imgui.h>

namespace maku::ui {

static maku::app::PageId NavPage(int i) { return static_cast<maku::app::PageId>(i); }

void InitAppShell() {
    auto& app = maku::app::Application::Instance();
    ui::ApplyTheme(app.GetSettings().theme);
    app.RefreshGlassPipeline();
}

static void DrawUpdateDialog() {
    auto& app = maku::app::Application::Instance();
    if (!app.ShowUpdateDialog()) return;
    auto& l = app.L10n();
    ImGui::OpenPopup("UpdateAvailable");
    if (ImGui::BeginPopupModal("UpdateAvailable", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", l.Def("updatedialog").c_str());
        static bool dontShow{};
        ImGui::Checkbox(l.Def("updatecheckb").c_str(), &dontShow);
        if (ImGui::Button(l.Def("updatebutton").c_str())) {
            proc::OpenUrl(L"https://adderly.top/mt");
            ImGui::CloseCurrentPopup();
            app.SetShowUpdateDialog(false);
        }
        ImGui::SameLine();
        if (ImGui::Button(l.Def("updatecancel").c_str())) {
            if (dontShow) {
                app.GetSettings().disableUpdateNotify = true;
                app.GetSettings().Save();
            }
            ImGui::CloseCurrentPopup();
            app.SetShowUpdateDialog(false);
        }
        ImGui::EndPopup();
    }
}

void DrawAppShell() {
    auto& app = maku::app::Application::Instance();
    auto& l = app.L10n();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoBackground);

    const bool dark = ui::IsDarkTheme(app.GetSettings().theme);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::NavPaneColor(dark));
    ImGui::BeginChild("Sidebar", ImVec2(300, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushFont(FontTitle());
    ImGui::Text("%s", brand::kDisplayNameUtf8);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    const char* keys[] = {"expl", "wu",   "sr",  "per",  "uwp", "quick", "adv",
                          "compon", "act", "perf", "sat",  "procmgr", "pci"};
    for (int i = 0; i < 13; ++i) {
        std::string label = l.CatName(keys[i]);
        const bool selected = app.CurrentPage() == NavPage(i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, ui::AccentTextColor());
        if (ImGui::Selectable(label.c_str(), selected, 0, ImVec2(0, 40)))
            app.SetPage(NavPage(i));
        if (selected) ImGui::PopStyleColor();
    }
    ImGui::Separator();
    if (ImGui::Selectable(l.Get("base", "lowtabs", "rexp").c_str(), false))
        app.RequestExplorerRestart();
    {
        const bool settingsSel = app.CurrentPage() == maku::app::PageId::Settings;
        if (settingsSel) ImGui::PushStyleColor(ImGuiCol_Text, ui::AccentTextColor());
        if (ImGui::Selectable(l.Get("base", "lowtabs", "set").c_str(), settingsSel))
            app.SetPage(maku::app::PageId::Settings);
        if (settingsSel) ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine(0.f, 14.f);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::ContentPaneColor(dark));
    ImGui::BeginChild("Content", ImVec2(0, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    DrawSearchBar();
    ImGui::Spacing();
    pages::Draw(app.CurrentPage());
    ImGui::EndChild();
    ImGui::PopStyleColor();

    DrawUpdateDialog();
    ImGui::End();
}

} // namespace maku::ui
