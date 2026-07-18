#pragma once

#include <windows.h>

#include <imgui.h>

#include <string>

namespace maku::ui {

/// Maps legacy "Standard"/"Mica" to "Dark"; unknown values become "Dark".
std::string NormalizeTheme(std::string theme);

bool IsDarkTheme(const std::string& theme);

ImVec4 AccentColor();
ImVec4 AccentLightColor();
ImVec4 AccentTextColor();
ImU32 AccentSparklineU32();
ImU32 AccentBarU32();

ImVec4 NavPaneColor(bool dark);
ImVec4 ContentPaneColor(bool dark);
ImVec4 CardSurfaceColor(bool dark);
ImVec4 CardGlassColor(bool dark);
ImVec4 CardBorderColor(bool dark);
ImU32 CardBorderU32(bool dark);

/// Stroke around metric tiles, search field, monitor sidebar cards, etc.
void DrawCardBorder(ImDrawList* dl, ImVec2 min, ImVec2 max, bool dark, float rounding,
                    bool accentHighlight = false);

const char* DwmGlassModeName();

void ApplyTheme(const std::string& theme, HWND hwnd = nullptr); // "Light" or "Dark"

void SyncDwmTheme(HWND hwnd, const std::string& theme, bool perPixelAlphaSwapChain = false);

void FrameClearColor(const std::string& theme, bool alphaSwapChain, float rgba[4]);

/// Reload fonts + ImGui/ImPlot metrics after WM_DPICHANGED or display change.
void OnDpiChanged(HWND hwnd, const std::string& theme, bool alphaSwapChain = false);

float UiScale();
float SidebarWidth();
float NavItemHeight();
float ContentGap();
float TileRounding();
float CardRounding();

void PushCardSurface(bool dark);
void PopCardSurface();

/// Rounded nav pill colors (call PushNavItemStyle / PopNavItemStyle).
void PushNavItemStyle(bool selected, bool dark);
void PopNavItemStyle(bool selected);

} // namespace maku::ui
