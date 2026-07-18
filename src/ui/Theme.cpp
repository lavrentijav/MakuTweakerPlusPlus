#include "ui/Theme.h"
#include "ui/Dpi.h"
#include "ui/Fonts.h"

#include <imgui.h>
#include <implot.h>

#include <cmath>
#include <cstdio>
#include <dwmapi.h>

namespace maku::ui {

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMSBT_DISABLE
#define DWMSBT_DISABLE 1
#endif
#ifndef DWMSBT_TABBEDWINDOW
#define DWMSBT_TABBEDWINDOW 4
#endif

namespace {

constexpr ImVec4 kFallbackAccent(0.86f, 0.72f, 0.28f, 1.00f);

static ImVec4 g_accent = kFallbackAccent;
static ImVec4 g_accentLight = kFallbackAccent;

static char g_dwmGlassModeBuf[128] = "off";
const char* g_dwmGlassMode = g_dwmGlassModeBuf;
static float g_uiScale = 1.f;

static void ScaleStyleFromBaseline(const float dpi) {
    ImGui::GetStyle().ScaleAllSizes(dpi);
    g_uiScale = dpi;
}

void SetDwmGlassMode(const char* text) {
    snprintf(g_dwmGlassModeBuf, sizeof(g_dwmGlassModeBuf), "%s", text ? text : "off");
    g_dwmGlassMode = g_dwmGlassModeBuf;
}

static ImVec4 ColorRefToImVec4(const DWORD colorRef) {
    return ImVec4(static_cast<float>((colorRef >> 0) & 0xFF) / 255.f,
                  static_cast<float>((colorRef >> 8) & 0xFF) / 255.f,
                  static_cast<float>((colorRef >> 16) & 0xFF) / 255.f, 1.f);
}

static ImVec4 ArgbToImVec4(const DWORD argb) {
    return ImVec4(static_cast<float>((argb >> 16) & 0xFF) / 255.f,
                  static_cast<float>((argb >> 8) & 0xFF) / 255.f,
                  static_cast<float>((argb >> 0) & 0xFF) / 255.f, 1.f);
}

static ImVec4 LightenAccent(const ImVec4& base, const float amount) {
    return ImVec4(base.x + (1.f - base.x) * amount, base.y + (1.f - base.y) * amount,
                  base.z + (1.f - base.z) * amount, 1.f);
}

static bool ReadAccentDword(const wchar_t* valueName, DWORD* out) {
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS st = RegGetValueW(
        HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Accent",
        valueName, RRF_RT_REG_DWORD, nullptr, &value, &size);
    if (st != ERROR_SUCCESS || value == 0) return false;
    *out = value;
    return true;
}

static void ReloadSystemAccent() {
    DWORD accentDword = 0;
    if (!ReadAccentDword(L"AccentColorMenu", &accentDword))
        ReadAccentDword(L"AccentColor", &accentDword);

    if (accentDword != 0) {
        g_accent = ColorRefToImVec4(accentDword);
    } else {
        DWORD dwm = 0;
        BOOL blend = FALSE;
        if (SUCCEEDED(DwmGetColorizationColor(&dwm, &blend)) && dwm != 0)
            g_accent = ArgbToImVec4(dwm);
        else
            g_accent = kFallbackAccent;
    }
    g_accentLight = LightenAccent(g_accent, 0.34f);
}

void ApplyModernFluentStyle(bool dark) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    const ImVec4 accent = g_accent;
    const ImVec4 accentLight = g_accentLight;

    s.AntiAliasedLines = true;
    s.AntiAliasedLinesUseTex = false;
    s.AntiAliasedFill = true;

    const float tileR = 22.f;
    s.WindowRounding = tileR;
    s.ChildRounding = tileR;
    s.PopupRounding = 20.f;
    s.FrameRounding = 18.f;
    s.TabRounding = 16.f;
    s.GrabRounding = 12.f;
    s.ScrollbarRounding = 12.f;
    s.ScrollbarSize = 10.f;

    s.FramePadding = ImVec2(16.f, 11.f);
    s.ItemSpacing = ImVec2(12.f, 10.f);
    s.ItemInnerSpacing = ImVec2(10.f, 7.f);
    s.WindowPadding = ImVec2(20.f, 18.f);
    s.CellPadding = ImVec2(12.f, 8.f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 0.f;
    s.FrameBorderSize = 0.f;
    s.PopupBorderSize = 0.f;
    s.TabBorderSize = 0.f;

    if (dark) {
        const ImVec4 winBg(0.08f, 0.06f, 0.05f, 1.f);
        const ImVec4 childBg(0.11f, 0.09f, 0.08f, 1.f);
        const ImVec4 popupBg(0.13f, 0.11f, 0.09f, 0.98f);
        const ImVec4 frame(0.18f, 0.15f, 0.13f, 1.f);
        const ImVec4 frameHover(0.24f, 0.20f, 0.17f, 1.f);
        const ImVec4 frameActive(0.30f, 0.25f, 0.20f, 1.f);

        c[ImGuiCol_WindowBg] = winBg;
        c[ImGuiCol_ChildBg] = childBg;
        c[ImGuiCol_PopupBg] = popupBg;
        c[ImGuiCol_Border] = ImVec4(1.f, 1.f, 1.f, 0.08f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);

        c[ImGuiCol_Text] = ImVec4(0.98f, 0.98f, 0.99f, 0.92f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.98f, 0.98f, 0.99f, 0.42f);

        c[ImGuiCol_FrameBg] = frame;
        c[ImGuiCol_FrameBgHovered] = frameHover;
        c[ImGuiCol_FrameBgActive] = frameActive;
        c[ImGuiCol_Button] = frame;
        c[ImGuiCol_ButtonHovered] = frameHover;
        c[ImGuiCol_ButtonActive] = frameActive;

        c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.32f);
        c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.42f);

        c[ImGuiCol_CheckMark] = accentLight;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accentLight;
        c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);

        c[ImGuiCol_Tab] = ImVec4(0.10f, 0.08f, 0.07f, 1.f);
        c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.40f);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.06f, 0.05f, 1.f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(accent.x, accent.y, accent.z, 0.32f);

        c[ImGuiCol_Separator] = ImVec4(0.40f, 0.35f, 0.30f, 0.45f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
        c[ImGuiCol_SeparatorActive] = accent;

        c[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.55f, 0.60f, 0.45f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.65f, 0.65f, 0.70f, 0.55f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.75f, 0.75f, 0.80f, 0.65f);

        c[ImGuiCol_TitleBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.f, 0.f, 0.f, 0.f);

        c[ImGuiCol_TableHeaderBg] = ImVec4(0.16f, 0.16f, 0.19f, 1.f);
        c[ImGuiCol_TableBorderStrong] = ImVec4(0.38f, 0.38f, 0.43f, 0.55f);
        c[ImGuiCol_TableBorderLight] = ImVec4(0.30f, 0.30f, 0.34f, 0.40f);
        c[ImGuiCol_TableRowBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TableRowBgAlt] = ImVec4(0.12f, 0.12f, 0.14f, 0.55f);

        c[ImGuiCol_NavHighlight] = accentLight;
        c[ImGuiCol_ResizeGrip] = ImVec4(0.50f, 0.50f, 0.55f, 0.20f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
        c[ImGuiCol_ResizeGripActive] = accent;
    } else {
        const ImVec4 winBg(0.97f, 0.97f, 0.98f, 1.f);
        const ImVec4 childBg(0.99f, 0.99f, 1.00f, 1.f);
        const ImVec4 popupBg(0.99f, 0.99f, 1.00f, 0.98f);
        const ImVec4 frame(0.93f, 0.93f, 0.95f, 1.f);
        const ImVec4 frameHover(0.88f, 0.88f, 0.91f, 1.f);
        const ImVec4 frameActive(0.82f, 0.82f, 0.86f, 1.f);

        c[ImGuiCol_WindowBg] = winBg;
        c[ImGuiCol_ChildBg] = childBg;
        c[ImGuiCol_PopupBg] = popupBg;
        c[ImGuiCol_Border] = ImVec4(0.72f, 0.72f, 0.76f, 0.55f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);

        c[ImGuiCol_Text] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.08f, 0.08f, 0.10f, 0.45f);

        c[ImGuiCol_FrameBg] = frame;
        c[ImGuiCol_FrameBgHovered] = frameHover;
        c[ImGuiCol_FrameBgActive] = frameActive;
        c[ImGuiCol_Button] = frame;
        c[ImGuiCol_ButtonHovered] = frameHover;
        c[ImGuiCol_ButtonActive] = frameActive;

        c[ImGuiCol_Header] = ImVec4(accent.x, accent.y, accent.z, 0.18f);
        c[ImGuiCol_HeaderHovered] = ImVec4(accent.x, accent.y, accent.z, 0.28f);
        c[ImGuiCol_HeaderActive] = ImVec4(accent.x, accent.y, accent.z, 0.38f);

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accentLight;
        c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.22f);

        c[ImGuiCol_Tab] = ImVec4(0.94f, 0.94f, 0.96f, 1.f);
        c[ImGuiCol_TabHovered] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = ImVec4(0.96f, 0.96f, 0.98f, 1.f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(accent.x, accent.y, accent.z, 0.20f);

        c[ImGuiCol_Separator] = ImVec4(0.68f, 0.68f, 0.72f, 0.55f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(accent.x, accent.y, accent.z, 0.45f);
        c[ImGuiCol_SeparatorActive] = accent;

        c[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_ScrollbarGrab] = ImVec4(0.55f, 0.55f, 0.60f, 0.35f);
        c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.45f, 0.50f, 0.50f);
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.35f, 0.35f, 0.40f, 0.60f);

        c[ImGuiCol_TitleBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.f, 0.f, 0.f, 0.f);

        c[ImGuiCol_TableHeaderBg] = ImVec4(0.94f, 0.94f, 0.96f, 1.f);
        c[ImGuiCol_TableBorderStrong] = ImVec4(0.72f, 0.72f, 0.76f, 0.65f);
        c[ImGuiCol_TableBorderLight] = ImVec4(0.80f, 0.80f, 0.84f, 0.45f);
        c[ImGuiCol_TableRowBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
        c[ImGuiCol_TableRowBgAlt] = ImVec4(0.95f, 0.95f, 0.97f, 0.65f);

        c[ImGuiCol_NavHighlight] = accent;
        c[ImGuiCol_ResizeGrip] = ImVec4(0.55f, 0.55f, 0.60f, 0.18f);
        c[ImGuiCol_ResizeGripHovered] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
        c[ImGuiCol_ResizeGripActive] = accent;
    }
}

} // namespace

std::string NormalizeTheme(std::string theme) {
    if (theme == "Standard" || theme == "Mica") return "Dark";
    if (theme != "Light" && theme != "Dark") return "Dark";
    return theme;
}

bool IsDarkTheme(const std::string& theme) { return NormalizeTheme(theme) == "Dark"; }

ImVec4 AccentColor() { return g_accent; }

ImVec4 AccentLightColor() { return g_accentLight; }

ImVec4 AccentTextColor() { return g_accentLight; }

ImU32 AccentSparklineU32() {
    const ImVec4 a = AccentLightColor();
    return IM_COL32(static_cast<int>(a.x * 255), static_cast<int>(a.y * 255),
                    static_cast<int>(a.z * 255), 255);
}

ImU32 AccentBarU32() {
    const ImVec4 a = AccentColor();
    return IM_COL32(static_cast<int>(a.x * 255), static_cast<int>(a.y * 255),
                    static_cast<int>(a.z * 255), 220);
}

static void ApplyImPlotColors(bool dark) {
    ImPlotStyle& ps = ImPlot::GetStyle();
    const ImVec4 accent = AccentColor();
    const ImVec4 grid = dark ? ImVec4(0.35f, 0.30f, 0.26f, 0.35f) : ImVec4(0.55f, 0.50f, 0.45f, 0.40f);
    const ImVec4 axis = dark ? ImVec4(0.75f, 0.72f, 0.68f, 0.85f) : ImVec4(0.25f, 0.22f, 0.18f, 0.90f);
    ps.Colors[ImPlotCol_PlotBg] = dark ? ImVec4(0.10f, 0.08f, 0.07f, 1.f) : ImVec4(0.98f, 0.97f, 0.95f, 1.f);
    ps.Colors[ImPlotCol_FrameBg] = CardSurfaceColor(dark);
    ps.Colors[ImPlotCol_PlotBorder] = ImVec4(1.f, 1.f, 1.f, dark ? 0.06f : 0.08f);
    ps.Colors[ImPlotCol_LegendBg] = ImVec4(0.f, 0.f, 0.f, dark ? 0.45f : 0.12f);
    ps.Colors[ImPlotCol_LegendBorder] = ImVec4(accent.x, accent.y, accent.z, 0.55f);
    ps.Colors[ImPlotCol_LegendText] = axis;
    ps.Colors[ImPlotCol_TitleText] = axis;
    ps.Colors[ImPlotCol_InlayText] = axis;
    ps.Colors[ImPlotCol_AxisText] = axis;
    ps.Colors[ImPlotCol_AxisGrid] = grid;
    ps.Colors[ImPlotCol_AxisTick] = grid;
    ps.Colors[ImPlotCol_Line] = accent;
    ps.Colors[ImPlotCol_Fill] = ImVec4(accent.x, accent.y, accent.z, 0.25f);
    ps.Colors[ImPlotCol_MarkerOutline] = accent;
    ps.Colors[ImPlotCol_MarkerFill] = AccentLightColor();
    ps.Colors[ImPlotCol_ErrorBar] = ImVec4(0.85f, 0.35f, 0.30f, 1.f);
    ps.Colors[ImPlotCol_Crosshairs] = ImVec4(accent.x, accent.y, accent.z, 0.65f);
    ps.Colors[ImPlotCol_Selection] = ImVec4(accent.x, accent.y, accent.z, 0.35f);
}

ImVec4 NavPaneColor(bool dark) {
    return dark ? ImVec4(0.06f, 0.05f, 0.04f, 1.f) : ImVec4(0.93f, 0.91f, 0.88f, 1.f);
}

ImVec4 ContentPaneColor(bool dark) {
    return dark ? ImVec4(0.09f, 0.07f, 0.06f, 1.f) : ImVec4(0.98f, 0.97f, 0.95f, 1.f);
}

ImVec4 CardSurfaceColor(bool dark) {
    return dark ? ImVec4(0.14f, 0.11f, 0.09f, 1.f) : ImVec4(0.96f, 0.94f, 0.91f, 1.f);
}

ImVec4 CardGlassColor(const bool dark) {
    return dark ? ImVec4(0.20f, 0.16f, 0.12f, 0.78f) : ImVec4(0.97f, 0.95f, 0.91f, 0.88f);
}

ImVec4 CardBorderColor(const bool dark) {
    return dark ? ImVec4(1.f, 1.f, 1.f, 0.10f) : ImVec4(0.f, 0.f, 0.f, 0.08f);
}

ImU32 CardBorderU32(const bool dark) {
    return ImGui::ColorConvertFloat4ToU32(CardBorderColor(dark));
}

void DrawCardBorder(ImDrawList* dl, const ImVec2 min, const ImVec2 max, const bool dark,
                    const float rounding, const bool accentHighlight) {
    if (!accentHighlight) return;
    dl->AddRect(min, max, AccentBarU32(), rounding, 0, 1.2f * g_uiScale);
}

const char* DwmGlassModeName() { return g_dwmGlassMode; }

void ApplyTheme(const std::string& theme, HWND hwnd) {
    ReloadSystemAccent();
    const std::string t = NormalizeTheme(theme);
    const bool dark = t == "Dark";
    ImGui::StyleColorsDark();
    if (!dark) ImGui::StyleColorsLight();
    ApplyModernFluentStyle(dark);
    ScaleStyleFromBaseline(GetDpiScale(hwnd));
    ApplyImPlotColors(dark);

    const float dpi = GetDpiScale(hwnd);
    ImPlotStyle& ps = ImPlot::GetStyle();
    ps.FitPadding = ImVec2(12.f * dpi, 12.f * dpi);
    ps.PlotPadding = ImVec2(16.f * dpi, 16.f * dpi);
    ps.LabelPadding = ImVec2(8.f * dpi, 8.f * dpi);
    ps.LegendPadding = ImVec2(10.f * dpi, 10.f * dpi);
    ps.AnnotationPadding = ImVec2(4.f * dpi, 4.f * dpi);
    ps.MajorTickLen = ImVec2(8.f * dpi, 8.f * dpi);
    ps.MinorTickLen = ImVec2(4.f * dpi, 4.f * dpi);
    ps.PlotBorderSize = 0.f;
}

float UiScale() { return g_uiScale; }

float SidebarWidth() { return 300.f * g_uiScale; }

float NavItemHeight() { return 40.f * g_uiScale; }

float ContentGap() { return 16.f * g_uiScale; }

float TileRounding() { return 22.f * g_uiScale; }

float CardRounding() { return 22.f * g_uiScale; }

void PushCardSurface(const bool dark) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CardSurfaceColor(dark));
    ImGui::PushStyleColor(ImGuiCol_Border, CardBorderColor(dark));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, CardRounding());
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f * g_uiScale, 14.f * g_uiScale));
}

void PopCardSurface() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void PushNavItemStyle(const bool selected, const bool dark) {
    if (!selected) return;
    const ImVec4 a = AccentColor();
    const ImVec4 pill = dark ? ImVec4(a.x, a.y, a.z, 0.28f) : ImVec4(a.x, a.y, a.z, 0.18f);
    ImGui::PushStyleColor(ImGuiCol_Header, pill);
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, pill);
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, pill);
}

void PopNavItemStyle(const bool selected) {
    if (!selected) return;
    ImGui::PopStyleColor(3);
}

void OnDpiChanged(HWND hwnd, const std::string& theme, const bool alphaSwapChain) {
    ui::ReloadFonts(hwnd);
    ApplyTheme(theme, hwnd);
    SyncDwmTheme(hwnd, theme, alphaSwapChain);
}

void SyncDwmTheme(HWND hwnd, const std::string& theme, bool /*perPixelAlphaSwapChain*/) {
    if (!hwnd) return;

    const std::string t = NormalizeTheme(theme);
    const bool dark = t == "Dark";

    SetDwmGlassMode(dark ? "dark chrome" : "light chrome");

    BOOL darkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    const int backdrop = DWMSBT_DISABLE;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

    const MARGINS margins{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    const int round = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof(round));
}

void FrameClearColor(const std::string& theme, bool /*alphaSwapChain*/, float rgba[4]) {
    const bool dark = IsDarkTheme(theme);
    if (dark) {
        rgba[0] = 0.08f;
        rgba[1] = 0.06f;
        rgba[2] = 0.05f;
        rgba[3] = 1.f;
    } else {
        rgba[0] = 0.97f;
        rgba[1] = 0.97f;
        rgba[2] = 0.98f;
        rgba[3] = 1.f;
    }
}

} // namespace maku::ui
