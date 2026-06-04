#pragma once
#include "app/Application.h"
#include <functional>
#include <string>

namespace maku::ui {

void PageTitle(const std::string& text);
bool ToggleRow(const char* id, const std::string& label, bool* value,
               std::function<void(bool)> onChanged = nullptr);
bool ButtonRow(const char* id, const std::string& label, std::function<void()> onClick = nullptr);
void OnOffLabels();
void TextWide(const std::wstring& text);

} // namespace maku::ui
