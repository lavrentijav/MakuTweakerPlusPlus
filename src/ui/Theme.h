#pragma once
#include <windows.h>
#include <string>

struct ImGuiStyle;

namespace maku::ui {

void ApplyTheme(const std::string& theme); // "Dark" or "Light"
void SyncDwmDark(HWND hwnd, bool dark);

} // namespace maku::ui
