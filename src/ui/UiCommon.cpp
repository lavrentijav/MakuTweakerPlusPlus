#include "ui/UiCommon.h"
#include "app/Application.h"
#include "ui/Fonts.h"
#include "ui/Theme.h"
#include "core/StringUtil.h"
#include <imgui.h>
#include <algorithm>
#include <windows.h>

namespace maku::ui {

bool BeginSettingsCard(const char* id, const bool dark, const float height) {
    PushCardSurface(dark);
    return ImGui::BeginChild(id, ImVec2(-1, height), ImGuiChildFlags_AlwaysUseWindowPadding);
}

void EndSettingsCard() {
    ImGui::EndChild();
    PopCardSurface();
}

void PageTitle(const std::string& text) {
    ImGui::PushFont(FontTitle());
    ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Spacing();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(
        p, ImVec2(p.x + w, p.y), ImGui::GetColorU32(ImGuiCol_Separator), 1.5f);
    ImGui::Dummy(ImVec2(0.f, 10.f * UiScale()));
}

void PageTitleCompact(const std::string& text) {
    ImGui::PushFont(FontTitle());
    ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0.f, 4.f * UiScale()));
}

void PushCompactToolbarStyle() {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.f * UiScale(), 5.f * UiScale()));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f * UiScale(), 4.f * UiScale()));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(4.f * UiScale(), 3.f * UiScale()));
}

void PopCompactToolbarStyle() { ImGui::PopStyleVar(3); }

bool ToggleRow(const char* id, const std::string& label, bool* value,
               std::function<void(bool)> onChanged) {
    ImGui::PushID(id);
    const bool prev = *value;
    const float tileH = 48.f * UiScale();
    const ImVec2 tileSize(ImGui::GetContentRegionAvail().x, tileH);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool dark = IsDarkTheme(maku::app::Application::Instance().GetSettings().theme);
    const ImVec2 p1(p0.x + tileSize.x, p0.y + tileSize.y);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(CardSurfaceColor(dark)), TileRounding());
    ImGui::InvisibleButton("##tile", tileSize);
    const ImVec2 inner(p0.x + 16.f * UiScale(), p0.y + 12.f * UiScale());
    dl->AddText(inner, IM_COL32(240, 235, 225, 255), label.c_str());
    ImGui::SetCursorScreenPos(ImVec2(p0.x + tileSize.x - 44.f * UiScale(), p0.y + 14.f * UiScale()));
    const ImVec4 accent = AccentColor();
    ImGui::PushStyleColor(ImGuiCol_CheckMark, accent);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(accent.x, accent.y, accent.z, 0.22f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(accent.x, accent.y, accent.z, 0.32f));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(accent.x, accent.y, accent.z, 0.42f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.f * UiScale());
    const bool changed = ImGui::Checkbox("##t", value);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);
    ImGui::SetCursorScreenPos(ImVec2(p0.x, p0.y + tileSize.y + 8.f * UiScale()));
    if (changed && onChanged) onChanged(*value);
    ImGui::PopID();
    return changed && *value != prev;
}

bool ButtonRow(const char* id, const std::string& label, std::function<void()> onClick) {
    ImGui::PushID(id);
    const float w = ImGui::GetContentRegionAvail().x;
    const bool clicked = ImGui::Button(label.c_str(), ImVec2(w > 120.f ? w : -1.f, 0.f));
    if (clicked && onClick) onClick();
    ImGui::PopID();
    return clicked;
}

void OnOffLabels() {}

void TextWide(const std::wstring& text) {
    ImGui::TextUnformatted(util::ToUtf8(text).c_str());
}

namespace {

void DrawMetricCardIcon(ImDrawList* dl, ImVec2 center, MetricCardIcon kind, ImU32 col) {
    const float s = 7.f;
    switch (kind) {
    case MetricCardIcon::Cpu:
        dl->AddRect(ImVec2(center.x - s, center.y - s * 0.7f), ImVec2(center.x + s, center.y + s),
                    col, 2.f, 0, 1.4f);
        dl->AddLine(ImVec2(center.x - s, center.y - s * 0.15f), ImVec2(center.x + s, center.y - s * 0.15f),
                    col, 1.2f);
        break;
    case MetricCardIcon::Gpu:
        dl->AddRect(ImVec2(center.x - s, center.y - s * 0.55f), ImVec2(center.x + s, center.y + s * 0.55f),
                    col, 2.f, 0, 1.3f);
        dl->AddLine(ImVec2(center.x - s * 0.5f, center.y - s * 0.55f),
                    ImVec2(center.x + s * 0.5f, center.y + s * 0.55f), col, 1.2f);
        break;
    case MetricCardIcon::Ram:
        dl->AddRectFilled(ImVec2(center.x - s, center.y - s * 0.35f),
                          ImVec2(center.x + s, center.y + s * 0.35f), col, 2.f);
        dl->AddLine(ImVec2(center.x - s * 0.6f, center.y - s), ImVec2(center.x - s * 0.6f, center.y + s),
                    col, 1.2f);
        dl->AddLine(ImVec2(center.x + s * 0.6f, center.y - s), ImVec2(center.x + s * 0.6f, center.y + s),
                    col, 1.2f);
        break;
    case MetricCardIcon::Board:
        dl->AddRect(ImVec2(center.x - s, center.y - s), ImVec2(center.x + s, center.y + s), col, 2.f, 0,
                    1.3f);
        dl->AddCircleFilled(ImVec2(center.x - s * 0.45f, center.y - s * 0.45f), 2.f, col);
        dl->AddCircleFilled(ImVec2(center.x + s * 0.45f, center.y + s * 0.45f), 2.f, col);
        break;
    case MetricCardIcon::Storage:
        dl->AddRectFilled(ImVec2(center.x - s * 0.85f, center.y - s * 0.2f),
                          ImVec2(center.x + s * 0.85f, center.y + s), col, 2.f);
        dl->AddLine(ImVec2(center.x - s * 0.5f, center.y - s), ImVec2(center.x + s * 0.5f, center.y - s),
                    col, 1.3f);
        break;
    case MetricCardIcon::Device:
        dl->AddRect(ImVec2(center.x - s * 0.55f, center.y - s), ImVec2(center.x + s * 0.55f, center.y + s),
                    col, 2.f, 0, 1.3f);
        break;
    case MetricCardIcon::Generic:
    default:
        dl->AddCircleFilled(center, s * 0.55f, col);
        break;
    }
}

void DrawTitleActionGlyph(ImDrawList* dl, ImVec2 center, TitleActionIcon icon, ImU32 col) {
    const float s = 5.f;
    if (icon == TitleActionIcon::Save) {
        dl->AddLine(ImVec2(center.x, center.y - s), ImVec2(center.x, center.y + s * 0.6f), col, 1.5f);
        dl->AddLine(ImVec2(center.x - s * 0.7f, center.y + s * 0.15f),
                    ImVec2(center.x + s * 0.7f, center.y + s * 0.15f), col, 1.5f);
        dl->AddLine(ImVec2(center.x - s * 0.45f, center.y + s * 0.55f),
                    ImVec2(center.x + s * 0.45f, center.y + s * 0.55f), col, 1.5f);
    } else {
        dl->AddRect(ImVec2(center.x - s, center.y - s), ImVec2(center.x, center.y), col, 1.f, 0, 1.3f);
        dl->AddRect(ImVec2(center.x, center.y), ImVec2(center.x + s, center.y + s), col, 1.f, 0, 1.3f);
    }
}

} // namespace

void DrawMetricCard(const char* id, const char* label, const char* value, const MetricCardIcon icon,
                    const bool dark) {
    const float scale = UiScale();
    const float cardH = 76.f * scale;
    const float cardW = ImGui::GetContentRegionAvail().x;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 cardCol = ImGui::ColorConvertFloat4ToU32(CardSurfaceColor(dark));
    const ImVec2 p1(p0.x + cardW, p0.y + cardH);
    dl->AddRectFilled(p0, p1, cardCol, CardRounding());
    DrawMetricCardIcon(dl, ImVec2(p0.x + 22.f * scale, p0.y + cardH * 0.5f), icon, AccentBarU32());

    const ImU32 labelCol = IM_COL32(200, 195, 185, 255);
    const ImU32 valueCol = ImGui::ColorConvertFloat4ToU32(AccentTextColor());
    dl->AddText(ImVec2(p0.x + 44.f * scale, p0.y + 14.f * scale), labelCol, label);
    const ImVec2 valSize = ImGui::CalcTextSize(value);
    const float maxValW = cardW - 48.f * scale;
    if (valSize.x > maxValW) {
        ImGui::PushClipRect(ImVec2(p0.x + 44.f * scale, p0.y),
                            ImVec2(p0.x + cardW - 6.f * scale, p0.y + cardH), true);
    }
    dl->AddText(ImVec2(p0.x + 44.f * scale, p0.y + 36.f * scale), valueCol, value);
    if (valSize.x > maxValW) ImGui::PopClipRect();
    ImGui::InvisibleButton(id, ImVec2(cardW, cardH));
}

void DrawDetailRow(const char* label, const char* value) {
    ImGui::PushID(label);
    if (!ImGui::BeginTable("##detail_row", 2,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX)) {
        ImGui::PopID();
        return;
    }
    ImGui::TableSetupColumn("lbl", ImGuiTableColumnFlags_WidthStretch, 0.48f);
    ImGui::TableSetupColumn("val", ImGuiTableColumnFlags_WidthStretch, 0.52f);
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    const float colW = ImGui::GetContentRegionAvail().x;
    const ImVec2 valSize = ImGui::CalcTextSize(value);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.f, colW - valSize.x));
    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
    ImGui::EndTable();
    ImGui::Dummy(ImVec2(0.f, 4.f * UiScale()));
    ImGui::PopID();
}

namespace {

void DrawHeaderIcon(ImDrawList* dl, const ImVec2 center, const MetricCardIcon icon) {
    DrawMetricCardIcon(dl, center, icon, AccentBarU32());
}

} // namespace

void DrawSettingsCard(const char* id, const char* header, const char* value, const MetricCardIcon icon,
                      const bool /*dark*/) {
    const float scale = UiScale();
    const float rowH = 40.f * scale;
    ImGui::PushID(id);
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const float rowW = ImGui::GetContentRegionAvail().x;
    DrawHeaderIcon(ImGui::GetWindowDrawList(),
                   ImVec2(p0.x + 12.f * scale, p0.y + rowH * 0.5f), icon);
    ImGui::Dummy(ImVec2(26.f * scale, rowH));
    ImGui::SameLine(0.f, 4.f * scale);
    ImGui::BeginGroup();
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(205, 198, 185, 255));
    ImGui::TextUnformatted(header);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    ImGui::TextUnformatted(value);
    ImGui::PopStyleColor();
    ImGui::EndGroup();
    ImGui::Dummy(ImVec2(rowW, 0.f));
    ImGui::Dummy(ImVec2(0.f, 2.f * scale));
    ImGui::PopID();
}

bool BeginCollapsibleSection(const char* id, const char* title, const bool dark,
                             const bool defaultOpen, const MetricCardIcon headerIcon) {
    const float scale = UiScale();
    ImGui::PushID(id);
    ImGui::Dummy(ImVec2(0.f, 4.f * scale));

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.f, 0.f, 0.f, 0.f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                          dark ? ImVec4(1.f, 1.f, 1.f, 0.05f) : ImVec4(0.f, 0.f, 0.f, 0.04f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                          dark ? ImVec4(1.f, 1.f, 1.f, 0.08f) : ImVec4(0.f, 0.f, 0.f, 0.06f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.f * scale, 6.f * scale));

    const float headerH = ImGui::GetFrameHeight();
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    DrawHeaderIcon(ImGui::GetWindowDrawList(),
                   ImVec2(rowStart.x + 14.f * scale, rowStart.y + headerH * 0.5f), headerIcon);
    ImGui::Dummy(ImVec2(28.f * scale, headerH));
    ImGui::SameLine(0.f, 2.f * scale);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
    if (defaultOpen) flags |= ImGuiTreeNodeFlags_DefaultOpen;

    const bool open = ImGui::CollapsingHeader(title, flags);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if (!open) {
        ImGui::PopID();
        ImGui::Dummy(ImVec2(0.f, 6.f * scale));
        return false;
    }

    ImGui::Indent(10.f * scale);
    ImGui::PushItemWidth(-1.f);
    return true;
}

void EndCollapsibleSection() {
    const float scale = UiScale();
    ImGui::PopItemWidth();
    ImGui::Unindent(10.f * scale);
    ImGui::PopID();
    ImGui::Dummy(ImVec2(0.f, 6.f * scale));
}

bool DrawTitleActionButton(const char* id, const TitleActionIcon icon, const char* tooltip) {
    const float scale = UiScale();
    const float sz = 32.f * scale;
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(sz, sz));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();
    if (tooltip && hovered) ImGui::SetTooltip("%s", tooltip);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool dark = IsDarkTheme(maku::app::Application::Instance().GetSettings().theme);
    const ImU32 bg = ImGui::ColorConvertFloat4ToU32(
        hovered ? ImVec4(CardSurfaceColor(dark).x + 0.08f, CardSurfaceColor(dark).y + 0.08f,
                         CardSurfaceColor(dark).z + 0.08f, 1.f)
                : CardSurfaceColor(dark));
    const ImVec2 p1(p0.x + sz, p0.y + sz);
    dl->AddRectFilled(p0, p1, bg, 10.f * scale);
    if (hovered) DrawCardBorder(dl, p0, p1, dark, 10.f * scale, true);
    DrawTitleActionGlyph(dl, ImVec2(p0.x + sz * 0.5f, p0.y + sz * 0.52f), icon,
                         IM_COL32(220, 210, 180, 255));
    return clicked;
}

void PageTitleWithActions(const std::string& title, const char* saveTip, const char* copyTip,
                          std::function<void()> onSave, std::function<void()> onCopy) {
    const float scale = UiScale();
    ImGui::PushFont(FontTitle());
    ImGui::PushStyleColor(ImGuiCol_Text, AccentTextColor());
    ImGui::TextUnformatted(title.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();

    const ImVec2 titleMin = ImGui::GetItemRectMin();
    const ImVec2 titleMax = ImGui::GetItemRectMax();
    ImGui::SetCursorScreenPos(ImVec2(titleMax.x + 10.f * scale, titleMin.y + 2.f * scale));
    if (onSave && DrawTitleActionButton("##title_save", TitleActionIcon::Save, saveTip)) onSave();
    ImGui::SameLine(0.f, 6.f * scale);
    if (onCopy && DrawTitleActionButton("##title_copy", TitleActionIcon::Copy, copyTip)) onCopy();

    ImGui::Dummy(ImVec2(0.f, 6.f * scale));
}

bool SetClipboardText(const HWND owner, const std::wstring& text) {
    if (!OpenClipboard(owner)) return false;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) {
        CloseClipboard();
        return false;
    }
    void* locked = GlobalLock(mem);
    if (!locked) {
        GlobalFree(mem);
        CloseClipboard();
        return false;
    }
    memcpy(locked, text.c_str(), bytes);
    GlobalUnlock(mem);
    SetClipboardData(CF_UNICODETEXT, mem);
    CloseClipboard();
    return true;
}

} // namespace maku::ui
