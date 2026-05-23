#pragma once

#include <windows.h>

#include <imgui.h>

#include <string>

namespace maku::ui {

/// Maps legacy "Standard"/"Mica" to "Dark"; unknown values become "Dark".
std::string NormalizeTheme(std::string theme);

bool IsDarkTheme(const std::string& theme);

ImVec4 AccentTextColor();

ImVec4 NavPaneColor(bool dark);
ImVec4 ContentPaneColor(bool dark);
ImVec4 CardSurfaceColor(bool dark);

const char* DwmGlassModeName();

void ApplyTheme(const std::string& theme); // "Light" or "Dark"

void SyncDwmTheme(HWND hwnd, const std::string& theme, bool perPixelAlphaSwapChain = false);

void FrameClearColor(const std::string& theme, bool alphaSwapChain, float rgba[4]);

} // namespace maku::ui
