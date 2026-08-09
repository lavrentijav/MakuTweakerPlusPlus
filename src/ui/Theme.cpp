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

static bool g_darkTheme = true;
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

/// WinUI 3 / Fluent neutral palette, the same one ModernWPF and MicaWPF paint
/// the original with. Values are the documented theme resources flattened
/// against the window background so we never need real translucency: a solid
/// fill is one draw call, an acrylic blur is a render target plus a shader.
struct Palette {
    ImVec4 windowBg;      ///< SolidBackgroundFillColorBase
    ImVec4 paneBg;        ///< SolidBackgroundFillColorSecondary (nav pane)
    ImVec4 cardBg;        ///< CardBackgroundFillColorDefault
    ImVec4 control;       ///< ControlFillColorDefault
    ImVec4 controlHover;  ///< ControlFillColorSecondary
    ImVec4 controlActive; ///< ControlFillColorTertiary
    ImVec4 stroke;        ///< ControlStrokeColorDefault
    ImVec4 text;          ///< TextFillColorPrimary
    ImVec4 textSecondary; ///< TextFillColorSecondary
    ImVec4 textDisabled;  ///< TextFillColorDisabled
    ImVec4 subtle;        ///< SubtleFillColorSecondary (hovered list rows)
};

constexpr ImVec4 Rgb(int r, int g, int b, float a = 1.f) {
    return ImVec4(r / 255.f, g / 255.f, b / 255.f, a);
}

Palette ThemePalette(const bool dark) {
    if (dark) {
        return Palette{
            /*windowBg*/ Rgb(0x20, 0x20, 0x20),
            /*paneBg*/ Rgb(0x1C, 0x1C, 0x1C),
            /*cardBg*/ Rgb(0x2B, 0x2B, 0x2B),
            /*control*/ Rgb(0x2D, 0x2D, 0x2D),
            /*controlHover*/ Rgb(0x32, 0x32, 0x32),
            /*controlActive*/ Rgb(0x27, 0x27, 0x27),
            /*stroke*/ ImVec4(1.f, 1.f, 1.f, 0.093f),
            /*text*/ Rgb(0xFF, 0xFF, 0xFF),
            /*textSecondary*/ ImVec4(1.f, 1.f, 1.f, 0.786f),
            /*textDisabled*/ ImVec4(1.f, 1.f, 1.f, 0.364f),
            /*subtle*/ ImVec4(1.f, 1.f, 1.f, 0.061f),
        };
    }
    return Palette{
        /*windowBg*/ Rgb(0xF3, 0xF3, 0xF3),
        /*paneBg*/ Rgb(0xEE, 0xEE, 0xEE),
        /*cardBg*/ Rgb(0xFB, 0xFB, 0xFB),
        /*control*/ Rgb(0xFF, 0xFF, 0xFF),
        /*controlHover*/ Rgb(0xF9, 0xF9, 0xF9),
        /*controlActive*/ Rgb(0xF5, 0xF5, 0xF5),
        /*stroke*/ ImVec4(0.f, 0.f, 0.f, 0.0578f),
        /*text*/ ImVec4(0.f, 0.f, 0.f, 0.8956f),
        /*textSecondary*/ ImVec4(0.f, 0.f, 0.f, 0.6063f),
        /*textDisabled*/ ImVec4(0.f, 0.f, 0.f, 0.3628f),
        /*subtle*/ ImVec4(0.f, 0.f, 0.f, 0.0373f),
    };
}

static Palette g_palette = ThemePalette(true);

void ApplyModernFluentStyle(bool dark) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    g_palette = ThemePalette(dark);
    const Palette& p = g_palette;

    const ImVec4 accent = g_accent;
    const ImVec4 accentLight = g_accentLight;
    // Fluent tints the accent toward the text color so it stays readable on
    // whichever background it lands on.
    const ImVec4 accentOnSurface = dark ? accentLight : accent;

    s.AntiAliasedLines = true;
    s.AntiAliasedLinesUseTex = false;
    s.AntiAliasedFill = true;

    // Fluent corner radii: 4 for controls, 8 for cards, flyouts and the window.
    s.WindowRounding = 8.f;
    s.ChildRounding = 8.f;
    s.PopupRounding = 8.f;
    s.FrameRounding = 4.f;
    s.TabRounding = 6.f;
    s.GrabRounding = 4.f;
    s.ScrollbarRounding = 4.f;
    s.ScrollbarSize = 12.f;
    s.GrabMinSize = 12.f;

    // Metrics taken from the WinUI control sizes the WPF build uses: 32px tall
    // buttons, 8px gutters, 12px content padding.
    s.FramePadding = ImVec2(11.f, 6.f);
    s.ItemSpacing = ImVec2(8.f, 6.f);
    s.ItemInnerSpacing = ImVec2(8.f, 4.f);
    s.WindowPadding = ImVec2(16.f, 14.f);
    s.CellPadding = ImVec2(10.f, 6.f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 1.f;
    s.FrameBorderSize = 1.f;
    s.PopupBorderSize = 1.f;
    s.TabBorderSize = 0.f;

    c[ImGuiCol_WindowBg] = p.windowBg;
    c[ImGuiCol_ChildBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_PopupBg] = p.cardBg;
    c[ImGuiCol_Border] = p.stroke;
    c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);

    c[ImGuiCol_Text] = p.text;
    c[ImGuiCol_TextDisabled] = p.textDisabled;

    c[ImGuiCol_FrameBg] = p.control;
    c[ImGuiCol_FrameBgHovered] = p.controlHover;
    c[ImGuiCol_FrameBgActive] = p.controlActive;
    c[ImGuiCol_Button] = p.control;
    c[ImGuiCol_ButtonHovered] = p.controlHover;
    c[ImGuiCol_ButtonActive] = p.controlActive;

    c[ImGuiCol_Header] = p.subtle;
    c[ImGuiCol_HeaderHovered] = p.subtle;
    c[ImGuiCol_HeaderActive] = p.controlActive;

    c[ImGuiCol_CheckMark] = dark ? ImVec4(0.f, 0.f, 0.f, 1.f) : ImVec4(1.f, 1.f, 1.f, 1.f);
    c[ImGuiCol_SliderGrab] = accentOnSurface;
    c[ImGuiCol_SliderGrabActive] = accentOnSurface;
    c[ImGuiCol_TextSelectedBg] = ImVec4(accent.x, accent.y, accent.z, 0.35f);

    c[ImGuiCol_Tab] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TabHovered] = p.subtle;
    c[ImGuiCol_TabActive] = p.cardBg;
    c[ImGuiCol_TabUnfocused] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TabUnfocusedActive] = p.control;

    c[ImGuiCol_Separator] = p.stroke;
    c[ImGuiCol_SeparatorHovered] = p.stroke;
    c[ImGuiCol_SeparatorActive] = accentOnSurface;

    c[ImGuiCol_ScrollbarBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ScrollbarGrab] = dark ? ImVec4(1.f, 1.f, 1.f, 0.33f) : ImVec4(0.f, 0.f, 0.f, 0.33f);
    c[ImGuiCol_ScrollbarGrabHovered] =
        dark ? ImVec4(1.f, 1.f, 1.f, 0.45f) : ImVec4(0.f, 0.f, 0.f, 0.45f);
    c[ImGuiCol_ScrollbarGrabActive] =
        dark ? ImVec4(1.f, 1.f, 1.f, 0.55f) : ImVec4(0.f, 0.f, 0.f, 0.55f);

    c[ImGuiCol_TitleBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(0.f, 0.f, 0.f, 0.f);

    c[ImGuiCol_TableHeaderBg] = p.paneBg;
    c[ImGuiCol_TableBorderStrong] = p.stroke;
    c[ImGuiCol_TableBorderLight] = p.stroke;
    c[ImGuiCol_TableRowBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_TableRowBgAlt] = p.subtle;

    c[ImGuiCol_NavHighlight] = accentOnSurface;
    c[ImGuiCol_ResizeGrip] = ImVec4(0.f, 0.f, 0.f, 0.f);
    c[ImGuiCol_ResizeGripHovered] = p.subtle;
    c[ImGuiCol_ResizeGripActive] = accentOnSurface;

    c[ImGuiCol_PlotLines] = accentOnSurface;
    c[ImGuiCol_PlotLinesHovered] = accentOnSurface;
    c[ImGuiCol_PlotHistogram] = accentOnSurface;
    c[ImGuiCol_PlotHistogramHovered] = accentLight;
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

ImVec4 AccentTextColor() {
    // Fluent uses AccentTextFillColorPrimary: the light-2 tint on dark
    // backgrounds, the base accent on light ones. Using the light tint on a
    // white page washes headings out to near-invisible.
    return g_darkTheme ? g_accentLight : g_accent;
}

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
    const Palette p = ThemePalette(dark);
    const ImVec4 accent = dark ? g_accentLight : g_accent;

    ps.Colors[ImPlotCol_PlotBg] = p.cardBg;
    ps.Colors[ImPlotCol_FrameBg] = ImVec4(0.f, 0.f, 0.f, 0.f);
    ps.Colors[ImPlotCol_PlotBorder] = p.stroke;
    ps.Colors[ImPlotCol_LegendBg] = p.cardBg;
    ps.Colors[ImPlotCol_LegendBorder] = p.stroke;
    ps.Colors[ImPlotCol_LegendText] = p.textSecondary;
    ps.Colors[ImPlotCol_TitleText] = p.text;
    ps.Colors[ImPlotCol_InlayText] = p.textSecondary;
    ps.Colors[ImPlotCol_AxisText] = p.textSecondary;
    ps.Colors[ImPlotCol_AxisGrid] = p.stroke;
    ps.Colors[ImPlotCol_AxisTick] = p.stroke;
    ps.Colors[ImPlotCol_Line] = accent;
    ps.Colors[ImPlotCol_Fill] = ImVec4(accent.x, accent.y, accent.z, 0.22f);
    ps.Colors[ImPlotCol_MarkerOutline] = accent;
    ps.Colors[ImPlotCol_MarkerFill] = accent;
    ps.Colors[ImPlotCol_ErrorBar] = ImVec4(0.85f, 0.35f, 0.30f, 1.f);
    ps.Colors[ImPlotCol_Crosshairs] = p.textSecondary;
    ps.Colors[ImPlotCol_Selection] = ImVec4(accent.x, accent.y, accent.z, 0.30f);
}

ImVec4 NavPaneColor(bool dark) { return ThemePalette(dark).paneBg; }

ImVec4 ContentPaneColor(bool dark) { return ThemePalette(dark).windowBg; }

ImVec4 CardSurfaceColor(bool dark) { return ThemePalette(dark).cardBg; }

ImVec4 CardGlassColor(const bool dark) { return ThemePalette(dark).cardBg; }

ImVec4 CardBorderColor(const bool dark) { return ThemePalette(dark).stroke; }

ImVec4 TextPrimaryColor(const bool dark) { return ThemePalette(dark).text; }

ImVec4 TextSecondaryColor(const bool dark) { return ThemePalette(dark).textSecondary; }

ImVec4 ControlFillColor(const bool dark) { return ThemePalette(dark).control; }

ImVec4 SubtleFillColor(const bool dark) { return ThemePalette(dark).subtle; }

ImU32 CardBorderU32(const bool dark) {
    return ImGui::ColorConvertFloat4ToU32(CardBorderColor(dark));
}

void DrawCardBorder(ImDrawList* dl, const ImVec2 min, const ImVec2 max, const bool dark,
                    const float rounding, const bool accentHighlight) {
    // Fluent surfaces always carry a hairline stroke; the accent variant is
    // only for the focused/selected card.
    const ImU32 color = accentHighlight ? AccentBarU32() : CardBorderU32(dark);
    dl->AddRect(min, max, color, rounding, 0, 1.f * g_uiScale);
}

const char* DwmGlassModeName() { return g_dwmGlassMode; }

void ApplyTheme(const std::string& theme, HWND hwnd) {
    ReloadSystemAccent();
    const std::string t = NormalizeTheme(theme);
    const bool dark = t == "Dark";
    g_darkTheme = dark;
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

float TileRounding() { return 4.f * g_uiScale; }

float CardRounding() { return 8.f * g_uiScale; }

void PushCardSurface(const bool dark) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, CardSurfaceColor(dark));
    ImGui::PushStyleColor(ImGuiCol_Border, CardBorderColor(dark));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, CardRounding());
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.f * g_uiScale, 12.f * g_uiScale));
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
    const ImVec4 bg = ThemePalette(IsDarkTheme(theme)).windowBg;
    rgba[0] = bg.x;
    rgba[1] = bg.y;
    rgba[2] = bg.z;
    rgba[3] = 1.f;
}

} // namespace maku::ui
