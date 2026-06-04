#include "ui/UiCommon.h"
#include "ui/Fonts.h"
#include "core/StringUtil.h"
#include <imgui.h>

namespace maku::ui {

void PageTitle(const std::string& text) {
    ImGui::PushFont(FontTitle());
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
}

bool ToggleRow(const char* id, const std::string& label, bool* value,
               std::function<void(bool)> onChanged) {
    ImGui::PushID(id);
    bool prev = *value;
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
    bool changed = ImGui::Checkbox("##t", value);
    if (changed && onChanged) onChanged(*value);
    ImGui::PopID();
    return changed && *value != prev;
}

bool ButtonRow(const char* id, const std::string& label, std::function<void()> onClick) {
    ImGui::PushID(id);
    bool clicked = ImGui::Button(label.c_str());
    if (clicked && onClick) onClick();
    ImGui::PopID();
    return clicked;
}

void OnOffLabels() {}

void TextWide(const std::wstring& text) {
    const std::string utf8 = util::ToUtf8(text);
    ImGui::TextUnformatted(utf8.c_str());
}

} // namespace maku::ui
