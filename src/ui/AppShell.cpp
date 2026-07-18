#include "ui/AppShell.h"
#include "ui/Pages.h"
#include "ui/Search.h"
#include "ui/Theme.h"
#include "ui/Fonts.h"
#include "ui/Modals.h"
#include "app/Application.h"
#include "app/Branding.h"
#include "core/Localization.h"
#include "core/ProcessRunner.h"
#include <imgui.h>

namespace maku::ui {
namespace {

static maku::app::PageId NavPage(int i) { return static_cast<maku::app::PageId>(i); }

static constexpr int kNavPageCount = 15;
static const char* kNavKeys[kNavPageCount] = {
    "expl", "wu",   "sr",  "per",  "uwp", "quick", "adv",  "compon", "act",
    "perf", "sat",  "procmgr", "pci", "wininfo", "mon"};

static void DrawNavItems(maku::app::Application& app, const l10n::Localization& l,
                         const bool horizontal, const bool useButtons, const bool dark) {
    const float itemH = NavItemHeight();
    for (int i = 0; i < kNavPageCount; ++i) {
        if (horizontal && i > 0) ImGui::SameLine(0.f, 6.f);
        ImGui::PushID(i);
        const std::string label = l.CatName(kNavKeys[i]);
        const bool selected = app.CurrentPage() == NavPage(i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());

        bool activated = false;
        if (useButtons) {
            if (selected) {
                const ImVec4 accent = AccentColor();
                const ImVec4 btn(accent.x, accent.y, accent.z, dark ? 0.55f : 0.42f);
                ImGui::PushStyleColor(ImGuiCol_Button, btn);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent);
            }
            activated = ImGui::Button(label.c_str());
            if (selected) ImGui::PopStyleColor(3);
        } else {
            const float w = ImGui::GetContentRegionAvail().x;
            PushNavItemStyle(selected, dark);
            activated = ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_None,
                                          ImVec2(w, itemH));
            PopNavItemStyle(selected);
            if (selected) {
                const ImVec2 mn = ImGui::GetItemRectMin();
                const ImVec2 mx = ImGui::GetItemRectMax();
                ImGui::GetWindowDrawList()->AddRectFilled(
                    ImVec2(mn.x, mn.y + 6.f), ImVec2(mn.x + 4.f, mx.y - 6.f), AccentBarU32(),
                    TileRounding() * 0.25f);
            }
        }
        if (activated) app.SetPage(NavPage(i));
        if (selected) ImGui::PopStyleColor();
        ImGui::PopID();
    }
}

static void DrawSidebarNav(maku::app::Application& app, const l10n::Localization& l,
                           const bool dark) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NavPaneColor(dark));
    ImGui::BeginChild("Sidebar", ImVec2(SidebarWidth(), 0),
                      ImGuiChildFlags_AlwaysUseWindowPadding);
    ImGui::PushFont(FontTitle());
    ImGui::Text("%s", brand::kDisplayNameUtf8);
    ImGui::PopFont();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushItemWidth(-1.f);
    DrawNavItems(app, l, false, false, dark);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const float w = ImGui::GetContentRegionAvail().x;
    const float itemH = NavItemHeight();
    if (ImGui::Selectable(l.Get("base", "lowtabs", "rexp").c_str(), false, 0, ImVec2(w, itemH)))
        app.RequestExplorerRestart();

    const bool settingsSel = app.CurrentPage() == maku::app::PageId::Settings;
    if (settingsSel) ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    PushNavItemStyle(settingsSel, dark);
    if (ImGui::Selectable(l.Get("base", "lowtabs", "set").c_str(), settingsSel, 0,
                          ImVec2(w, itemH)))
        app.SetPage(maku::app::PageId::Settings);
    PopNavItemStyle(settingsSel);
    if (settingsSel) {
        const ImVec2 mn = ImGui::GetItemRectMin();
        const ImVec2 mx = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(mn.x, mn.y + 6.f), ImVec2(mn.x + 4.f, mx.y - 6.f),
                                                  AccentBarU32(), TileRounding() * 0.25f);
        ImGui::PopStyleColor();
    }

    ImGui::PopItemWidth();
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

static void DrawVerticalSeparator() {
    const float h = ImGui::GetContentRegionAvail().y;
    if (h < 8.f) return;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y), ImVec2(p.x, p.y + h),
                                        ImGui::GetColorU32(ImGuiCol_Separator), 1.f);
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(1.f, 0.f));
}

static void DrawTopNavBar(maku::app::Application& app, const l10n::Localization& l,
                          const bool dark, const bool compact) {
    const float scale = UiScale();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, NavPaneColor(dark));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        ImVec2((compact ? 8.f : 10.f) * scale, (compact ? 3.f : 6.f) * scale));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f * scale, 0.f));

    const float rowH = ImGui::GetFrameHeight();
    const float padY = compact ? 6.f * scale : ImGui::GetStyle().WindowPadding.y * 2.f;
    const float barH = rowH + padY + (compact ? 2.f : ImGui::GetStyle().ScrollbarSize + 4.f);

    ImGui::BeginChild("TopNav", ImVec2(-1, barH), ImGuiChildFlags_AlwaysUseWindowPadding,
                      ImGuiWindowFlags_HorizontalScrollbar);

    ImGui::PushFont(FontTitle());
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(brand::kDisplayNameUtf8);
    ImGui::PopFont();
    ImGui::SameLine(0.f, 12.f);
    DrawVerticalSeparator();
    ImGui::SameLine(0.f, 10.f);

    DrawNavItems(app, l, true, true, dark);

    ImGui::SameLine(0.f, 12.f);
    DrawVerticalSeparator();
    ImGui::SameLine(0.f, 10.f);

    if (ImGui::Button(l.Get("base", "lowtabs", "rexp").c_str())) app.RequestExplorerRestart();
    ImGui::SameLine(0.f, 6.f);
    const bool settingsSel = app.CurrentPage() == maku::app::PageId::Settings;
    if (settingsSel) ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    if (ImGui::Button(l.Get("base", "lowtabs", "set").c_str()))
        app.SetPage(maku::app::PageId::Settings);
    if (settingsSel) ImGui::PopStyleColor();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
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

} // namespace

void InitAppShell() {
    auto& app = maku::app::Application::Instance();
    ApplyTheme(app.GetSettings().theme, app.Hwnd());
    SyncDwmTheme(app.Hwnd(), app.GetSettings().theme, false);
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

    const bool dark = IsDarkTheme(app.GetSettings().theme);
    const bool monitorMode = app.CurrentPage() == maku::app::PageId::Monitor;
    const bool exclusive = app.ExclusiveLayout();

    if (exclusive && app.CurrentPage() == maku::app::PageId::ProcessMgr) {
        if (ImGui::Button(l.Get("pmgr", "main", "getback").c_str())) {
            app.GetSettings().exclusiveMode = false;
            app.GetSettings().Save();
        }
        ImGui::Spacing();
    }

    if (monitorMode && !exclusive) {
        DrawTopNavBar(app, l, dark, true);
        PushCardSurface(dark);
        ImGui::BeginChild("Content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        DrawSearchBar(true);
        ImGui::Dummy(ImVec2(0.f, 2.f * UiScale()));
        pages::Draw(app.CurrentPage());
        ImGui::EndChild();
        PopCardSurface();
    } else if (!exclusive) {
        DrawSidebarNav(app, l, dark);
        ImGui::SameLine(0.f, ContentGap());
        PushCardSurface(dark);
        ImGui::BeginChild("Content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        DrawSearchBar();
        ImGui::Spacing();
        pages::Draw(app.CurrentPage());
        ImGui::EndChild();
        PopCardSurface();
    } else {
        PushCardSurface(dark);
        ImGui::BeginChild("Content", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        pages::Draw(app.CurrentPage());
        ImGui::EndChild();
        PopCardSurface();
    }

    DrawUpdateDialog();
    modals::DrawAll();
    ImGui::End();
}

} // namespace maku::ui
