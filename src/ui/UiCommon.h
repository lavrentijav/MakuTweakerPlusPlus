#pragma once
#include "app/Application.h"
#include <functional>
#include <string>

namespace maku::ui {

void PageTitle(const std::string& text);
void PageTitleCompact(const std::string& text);
void PushCompactToolbarStyle();
void PopCompactToolbarStyle();
/// Opens a Fluent card. The return value only says whether the contents are
/// visible — EndSettingsCard() must be called either way, exactly like
/// ImGui::BeginChild/EndChild.
bool BeginSettingsCard(const char* id, bool dark, float height = 0.f);
void EndSettingsCard();
bool ToggleRow(const char* id, const std::string& label, bool* value,
               std::function<void(bool)> onChanged = nullptr);
bool ButtonRow(const char* id, const std::string& label, std::function<void()> onClick = nullptr);
void OnOffLabels();
void TextWide(const std::wstring& text);

enum class MetricCardIcon {
    Cpu,
    Gpu,
    Ram,
    Board,
    Storage,
    Device,
    Generic,
};

enum class TitleActionIcon { Save, Copy };

void DrawMetricCard(const char* id, const char* label, const char* value, MetricCardIcon icon,
                    bool dark);
void DrawDetailRow(const char* label, const char* value);
void DrawSettingsCard(const char* id, const char* header, const char* value, MetricCardIcon icon,
                      bool dark);
bool BeginCollapsibleSection(const char* id, const char* title, bool dark, bool defaultOpen,
                             MetricCardIcon headerIcon = MetricCardIcon::Generic);
void EndCollapsibleSection();
bool DrawTitleActionButton(const char* id, TitleActionIcon icon, const char* tooltip);
void PageTitleWithActions(const std::string& title, const char* saveTip, const char* copyTip,
                          std::function<void()> onSave, std::function<void()> onCopy);
bool SetClipboardText(HWND owner, const std::wstring& text);

} // namespace maku::ui
