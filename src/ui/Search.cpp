#include "ui/Search.h"
#include "ui/Modals.h"
#include "ui/Theme.h"
#include "app/Application.h"
#include "core/ProcessRunner.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace maku::ui {
namespace {

static bool TrySearchEasterEgg(const std::string& query) {
    static const struct {
        const char* key;
        const wchar_t* url;
    } kEggs[] = {
        {"yuzuru", L"https://www.youtube.com/watch?v=Dnpyg6xOS2g&list=PLnz2Wc_QvOg8byWCAFy6uXkgOZuZwtv2P&index=1"},
        {"gabriel rondo", L"https://youtu.be/OXHOgIkO89I?si=CLqJUd0JCMFuQaKY&t=47"},
        {"yona", L"https://youtu.be/KyuSarQvt5U?si=g01af6NXKxQ6B2gR"},
        {"ahih", L"https://www.youtube.com/watch?v=IKZbwuwXTNY&list=PLnz2Wc_QvOg8byWCAFy6uXkgOZuZwtv2P&index=5&t=40"},
        {"kurumi", L"https://youtu.be/-DGBxD801jI?si=vPvodZ9gMoOg_aYA"},
    };
    for (const auto& egg : kEggs) {
        if (query == egg.key) {
            proc::OpenUrl(egg.url);
            return true;
        }
    }
    return false;
}

} // namespace

static std::string MapCategoryToTag(const std::string& internal) {
    if (internal == "expl") return "exp";
    if (internal == "sr") return "sys";
    if (internal == "procmgr") return "pmgr";
    if (internal == "wininfo") return "wininfo";
    if (internal == "makuyan_window") return "settings";
    return internal;
}

void DrawSearchBar(const bool compact) {
    auto& app = maku::app::Application::Instance();
    auto& l10n = app.L10n();
    static char query[256]{};
    const bool dark = IsDarkTheme(app.GetSettings().theme);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float scale = UiScale();
    const float h = (compact ? 30.f : 42.f) * scale;
    const float padX = (compact ? 10.f : 14.f) * scale;
    const float padY = (compact ? 6.f : 10.f) * scale;
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 p1(p0.x + w, p0.y + h);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(CardSurfaceColor(dark)), TileRounding());
    ImGui::Dummy(ImVec2(0.f, compact ? 2.f * scale : 4.f * scale));
    ImGui::SetCursorScreenPos(ImVec2(p0.x + padX, p0.y + padY));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, TileRounding());
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::SetNextItemWidth(w - padX * 2.f);
    ImGui::InputTextWithHint("##search", l10n.Def("search").c_str(), query, sizeof(query));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + h + (compact ? 3.f : 6.f) * scale));
    if (strlen(query) < 2) return;

    std::string q = query;
    for (auto& c : q) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    if (TrySearchEasterEgg(q)) {
        query[0] = 0;
        return;
    }

    const auto& tweaks = l10n.GetAllTweaksForSearch();

    if (ImGui::IsItemActive()) ImGui::OpenPopup("search_popup");
    if (ImGui::BeginPopup("search_popup")) {
        int shown = 0;
        for (const auto& t : tweaks) {
            std::string dn = t.displayName;
            for (auto& c : dn) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (dn.find(q) == std::string::npos) continue;
            if (shown++ > 10) break;
            if (ImGui::Selectable(t.displayName.c_str())) {
                if (t.internalCategoryTag == "makuyan_window")
                    ui::modals::OpenMakuYan();
                else
                    app.SetPage(maku::app::PageFromTag(MapCategoryToTag(t.internalCategoryTag)));
                query[0] = 0;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

} // namespace maku::ui
