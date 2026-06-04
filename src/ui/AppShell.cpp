#include "ui/AppShell.h"
#include "ui/Pages.h"
#include "ui/Search.h"
#include "ui/Theme.h"
#include "ui/Fonts.h"
#include "ui/Modals.h"
#include "app/Application.h"
#include "core/ProcessRunner.h"
#include "core/StringUtil.h"
#include <imgui.h>

namespace maku::ui {

static maku::app::PageId NavPage(int i) { return static_cast<maku::app::PageId>(i); }

void InitAppShell() {
    auto& app = maku::app::Application::Instance();
    ui::ApplyTheme(app.GetSettings().theme);
    ui::SyncDwmDark(app.Hwnd(), app.GetSettings().theme == "Dark");
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
    auto& s = app.GetSettings();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("Root", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);

    const bool exclusive = app.ExclusiveLayout();
    if (!exclusive) {
        ImGui::BeginChild("Sidebar", ImVec2(290, -40), true);
        ImGui::PushFont(FontTitle());
        ImGui::Text("MakuTweaker");
        ImGui::PopFont();
        ImGui::Separator();

        const char* keys[] = {"expl", "wu",   "sr",  "per",  "uwp", "quick", "adv",
                              "compon", "act", "perf", "sat",  "procmgr", "pci", "wininfo"};
        for (int i = 0; i < 14; ++i) {
            std::string label = l.CatName(keys[i]);
            if (ImGui::Selectable(label.c_str(), app.CurrentPage() == NavPage(i)))
                app.SetPage(NavPage(i));
        }
        ImGui::Separator();
        if (ImGui::Selectable(l.Get("base", "lowtabs", "rexp").c_str(), false))
            app.RequestExplorerRestart();
        if (ImGui::Selectable(l.Get("base", "lowtabs", "set").c_str(), false,
                              app.CurrentPage() == maku::app::PageId::Settings))
            app.SetPage(maku::app::PageId::Settings);
        ImGui::EndChild();
        ImGui::SameLine();
    } else if (app.CurrentPage() == maku::app::PageId::ProcessMgr) {
        if (ImGui::Button(l.Get("pmgr", "main", "getback").c_str())) {
            s.exclusiveMode = false;
            s.Save();
        }
    }

    ImGui::BeginChild("Content", ImVec2(0, -40), true);
    if (!exclusive) DrawSearchBar();
    ImGui::Spacing();
    pages::Draw(app.CurrentPage());
    ImGui::EndChild();

    DrawUpdateDialog();
    modals::DrawAll();
    ImGui::End();
}

} // namespace maku::ui
