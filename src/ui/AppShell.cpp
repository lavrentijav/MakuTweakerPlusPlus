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
#include <algorithm>

namespace maku::ui {

static maku::app::PageId NavPage(int i) { return static_cast<maku::app::PageId>(i); }

static constexpr int kNavPageCount = 14;
static const char* kNavKeys[kNavPageCount] = {
    "expl", "wu", "sr", "per", "uwp", "quick", "adv", "compon", "act", "perf", "sat", "procmgr", "pci", "mon"};

static void DrawNavItems(maku::app::Application& app, const l10n::Localization& l, bool horizontal,
                         bool useButtons) {
    for (int i = 0; i < kNavPageCount; ++i) {
        if (horizontal && i > 0) ImGui::SameLine(0.f, 4.f);
        ImGui::PushID(i);
        const std::string label = l.CatName(kNavKeys[i]);
        const bool selected = app.CurrentPage() == NavPage(i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, ui::AccentTextColor());
        bool activated = false;
        if (useButtons) {
            if (selected) {
                const ImVec4 accent{0.18f, 0.42f, 0.62f, 1.f};
                ImGui::PushStyleColor(ImGuiCol_Button, accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
            }
            activated = ImGui::Button(label.c_str());
            if (selected) ImGui::PopStyleColor(3);
        } else {
            const float w = ImGui::GetContentRegionAvail().x;
            activated =
                ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None, ImVec2(w, 40.f));
        }
        if (activated) app.SetPage(NavPage(i));
        if (selected) ImGui::PopStyleColor();
        ImGui::PopID();
    }
}

static void DrawSidebarNav(maku::app::Application& app, const l10n::Localization& l, bool dark) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::NavPaneColor(dark));
    ImGui::BeginChild("Sidebar", ImVec2(300, 0),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushFont(FontTitle());
    ImGui::Text("%s", brand::kDisplayNameUtf8);
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushItemWidth(-1.f);
    DrawNavItems(app, l, false, false);

    ImGui::Separator();
    {
        const float w = ImGui::GetContentRegionAvail().x;
        if (ImGui::Selectable(l.Get("base", "lowtabs", "rexp").c_str(), false, 0, ImVec2(w, 40.f)))
            app.RequestExplorerRestart();
    }
    {
        const bool settingsSel = app.CurrentPage() == maku::app::PageId::Settings;
        const float w = ImGui::GetContentRegionAvail().x;
        if (settingsSel) ImGui::PushStyleColor(ImGuiCol_Text, ui::AccentTextColor());
        if (ImGui::Selectable(l.Get("base", "lowtabs", "set").c_str(), settingsSel, 0,
                              ImVec2(w, 40.f)))
            app.SetPage(maku::app::PageId::Settings);
        if (settingsSel) ImGui::PopStyleColor();
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void DrawVerticalSeparator() {
    const float h = ImGui::GetFrameHeight();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + h),
                                        ImGui::GetColorU32(ImGuiCol_Separator), 1.f);
    ImGui::Dummy(ImVec2(1.f, h));
}

static void DrawTopNavBar(maku::app::Application& app, const l10n::Localization& l, bool dark) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::NavPaneColor(dark));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 5));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));

    const float rowH = ImGui::GetFrameHeight();
    const float padY = ImGui::GetStyle().WindowPadding.y * 2.f;
    const float borders = ImGui::GetStyle().ChildBorderSize * 2.f;
    const float scrollH = ImGui::GetStyle().ScrollbarSize;
    const float barH = rowH + padY + borders + scrollH + 2.f;

    ImGui::BeginChild("TopNav", ImVec2(-1, barH),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushFont(FontUi());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(brand::kDisplayNameUtf8);
    ImGui::PopFont();
    ImGui::SameLine(0.f, 10.f);
    DrawVerticalSeparator();
    ImGui::SameLine(0.f, 8.f);

    DrawNavItems(app, l, true, true);

    ImGui::SameLine(0.f, 10.f);
    DrawVerticalSeparator();
    ImGui::SameLine(0.f, 8.f);

    if (ImGui::Button(l.Get("base", "lowtabs", "rexp").c_str()))
        app.RequestExplorerRestart();
    ImGui::SameLine(0.f, 4.f);
    {
        const bool settingsSel = app.CurrentPage() == maku::app::PageId::Settings;
        if (settingsSel) ImGui::PushStyleColor(ImGuiCol_Text, ui::AccentTextColor());
        if (ImGui::Button(l.Get("base", "lowtabs", "set").c_str()))
            app.SetPage(maku::app::PageId::Settings);
        if (settingsSel) ImGui::PopStyleColor();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

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
    const bool monitorMode = app.CurrentPage() == maku::app::PageId::Monitor;

    if (monitorMode) {
        DrawTopNavBar(app, l, dark);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::ContentPaneColor(dark));
        ImGui::BeginChild("Content", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        pages::Draw(app.CurrentPage());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    } else {
        DrawSidebarNav(app, l, dark);
        ImGui::SameLine(0.f, 14.f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ui::ContentPaneColor(dark));
        ImGui::BeginChild("Content", ImVec2(0, 0),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding);
        DrawSearchBar();
        ImGui::Spacing();
        pages::Draw(app.CurrentPage());
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    DrawUpdateDialog();
    ImGui::End();
}

} // namespace maku::ui
