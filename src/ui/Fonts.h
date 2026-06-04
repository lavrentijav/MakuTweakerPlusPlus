#pragma once
#include <windows.h>

struct ImFont;

namespace maku::ui {

void InitFonts(HWND hwnd = nullptr);
void ReloadFonts(HWND hwnd = nullptr);
ImFont* FontUi();
ImFont* FontTitle();

} // namespace maku::ui
