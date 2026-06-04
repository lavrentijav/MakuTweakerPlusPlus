#include "ui/Search.h"
#include "app/Application.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace maku::ui {

static std::string MapCategoryToTag(const std::string& internal) {
    if (internal == "expl") return "exp";
    if (internal == "sr") return "sys";
    if (internal == "procmgr") return "pmgr";
    if (internal == "wininfo") return "wininfo";
    return internal;
}

void DrawSearchBar() {
    auto& app = maku::app::Application::Instance();
    auto& l10n = app.L10n();
    static char query[256]{};
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", l10n.Def("search").c_str(), query, sizeof(query));
    if (strlen(query) < 2) return;

    std::string q = query;
    for (auto& c : q) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    auto tweaks = l10n.GetAllTweaksForSearch();

    if (ImGui::IsItemActive()) ImGui::OpenPopup("search_popup");
    if (ImGui::BeginPopup("search_popup")) {
        int shown = 0;
        for (auto& t : tweaks) {
            std::string dn = t.displayName;
            for (auto& c : dn) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (dn.find(q) == std::string::npos) continue;
            if (shown++ > 10) break;
            if (ImGui::Selectable(t.displayName.c_str())) {
                app.SetPage(maku::app::PageFromTag(MapCategoryToTag(t.internalCategoryTag)));
                query[0] = 0;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }
}

} // namespace maku::ui
