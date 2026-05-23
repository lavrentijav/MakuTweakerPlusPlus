#pragma once
#include "app/Application.h"
#include <functional>
#include <string>

namespace maku::ui {

void PageTitle(const std::string& text);
bool ToggleRow(const char* id, const std::string& label, bool* value,
               std::function<void(bool)> onChanged = nullptr, bool requiresAdmin = false,
               bool rememberAcrossElevation = false);
bool ButtonRow(const char* id, const std::string& label, std::function<void()> onClick = nullptr,
               bool requiresAdmin = false,
               std::function<void()> rememberStateForElevation = nullptr);
void OnOffLabels();
void TextWide(const std::wstring& text);

/// Shows green confirmation for ~3s after a successful page save/apply.
void NotifyPageSaved(const char* pageKey, bool ok = true);
/// Footer: [Apply/Save] + status line (uses base.def "apply" / "applied").
void DrawSaveFooter(const char* pageKey, const std::function<bool()>& apply,
                    bool requiresAdmin = false,
                    std::function<void()> stageBeforeElevation = nullptr,
                    bool notifyOnApply = true);

} // namespace maku::ui
