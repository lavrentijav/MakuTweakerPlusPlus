#include "ui/Theme.h"

#include <imgui.h>

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

static char g_dwmGlassModeBuf[128] = "off";
const char* g_dwmGlassMode = g_dwmGlassModeBuf;

void SetDwmGlassMode(const char* text) {
    snprintf(g_dwmGlassModeBuf, sizeof(g_dwmGlassModeBuf), "%s", text ? text : "off");
    g_dwmGlassMode = g_dwmGlassModeBuf;
}

void ApplyModernFluentStyle(bool dark) {
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    const ImVec4 accent(0.00f, 0.47f, 0.87f, 1.00f);
    const ImVec4 accentLight(0.38f, 0.80f, 1.00f, 1.00f);

    s.WindowRounding = 10.f;
    s.ChildRounding = 10.f;
    s.PopupRounding = 10.f;
    s.FrameRounding = 6.f;
    s.TabRounding = 6.f;
    s.GrabRounding = 12.f;
    s.ScrollbarRounding = 12.f;
    s.ScrollbarSize = 14.f;

    s.FramePadding = ImVec2(12.f, 8.f);
    s.ItemSpacing = ImVec2(10.f, 8.f);
    s.ItemInnerSpacing = ImVec2(8.f, 6.f);
    s.WindowPadding = ImVec2(16.f, 16.f);
    s.WindowMenuButtonPosition = ImGuiDir_None;

    s.WindowBorderSize = 0.f;
    s.ChildBorderSize = 1.f;
    s.PopupBorderSize = 1.f;
    s.FrameBorderSize = 0.f;

    if (dark) {
        const ImVec4 winBg(0.10f, 0.10f, 0.11f, 1.f);
        const ImVec4 childBg(0.14f, 0.14f, 0.16f, 1.f);
        const ImVec4 popupBg(0.16f, 0.16f, 0.18f, 0.98f);
        const ImVec4 frame(0.20f, 0.20f, 0.24f, 1.f);
        const ImVec4 frameHover(0.26f, 0.26f, 0.31f, 1.f);
        const ImVec4 frameActive(0.32f, 0.32f, 0.38f, 1.f);

        c[ImGuiCol_WindowBg] = winBg;
        c[ImGuiCol_ChildBg] = childBg;
        c[ImGuiCol_PopupBg] = popupBg;
        c[ImGuiCol_Border] = ImVec4(0.42f, 0.42f, 0.48f, 0.35f);
        c[ImGuiCol_BorderShadow] = ImVec4(0.f, 0.f, 0.f, 0.f);

        c[ImGuiCol_Text] = ImVec4(0.98f, 0.98f, 0.99f, 0.92f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.98f, 0.98f, 0.99f, 0.42f);

        c[ImGuiCol_FrameBg] = frame;
        c[ImGuiCol_FrameBgHovered] = frameHover;
        c[ImGuiCol_FrameBgActive] = frameActive;
        c[ImGuiCol_Button] = frame;
        c[ImGuiCol_ButtonHovered] = frameHover;
        c[ImGuiCol_ButtonActive] = frameActive;

        c[ImGuiCol_Header] = ImVec4(0.00f, 0.47f, 0.87f, 0.28f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.38f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.47f, 0.87f, 0.48f);

        c[ImGuiCol_CheckMark] = accentLight;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = accentLight;
        c[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.47f, 0.87f, 0.35f);

        c[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.14f, 1.f);
        c[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.40f);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.12f, 1.f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.00f, 0.47f, 0.87f, 0.32f);

        c[ImGuiCol_Separator] = ImVec4(0.45f, 0.45f, 0.50f, 0.45f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.55f);
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
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.45f);
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

        c[ImGuiCol_Header] = ImVec4(0.00f, 0.47f, 0.87f, 0.18f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.28f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.00f, 0.47f, 0.87f, 0.38f);

        c[ImGuiCol_CheckMark] = accent;
        c[ImGuiCol_SliderGrab] = accent;
        c[ImGuiCol_SliderGrabActive] = ImVec4(0.00f, 0.40f, 0.78f, 1.f);
        c[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.47f, 0.87f, 0.22f);

        c[ImGuiCol_Tab] = ImVec4(0.94f, 0.94f, 0.96f, 1.f);
        c[ImGuiCol_TabHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.22f);
        c[ImGuiCol_TabActive] = accent;
        c[ImGuiCol_TabUnfocused] = ImVec4(0.96f, 0.96f, 0.98f, 1.f);
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.00f, 0.47f, 0.87f, 0.20f);

        c[ImGuiCol_Separator] = ImVec4(0.68f, 0.68f, 0.72f, 0.55f);
        c[ImGuiCol_SeparatorHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.45f);
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
        c[ImGuiCol_ResizeGripHovered] = ImVec4(0.00f, 0.47f, 0.87f, 0.35f);
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

ImVec4 AccentTextColor() { return ImVec4(0.38f, 0.80f, 1.00f, 1.00f); }

ImVec4 NavPaneColor(bool dark) {
    return dark ? ImVec4(0.08f, 0.08f, 0.09f, 1.f) : ImVec4(0.94f, 0.94f, 0.96f, 1.f);
}

ImVec4 ContentPaneColor(bool dark) {
    return dark ? ImVec4(0.12f, 0.12f, 0.14f, 1.f) : ImVec4(0.99f, 0.99f, 1.00f, 1.f);
}

ImVec4 CardSurfaceColor(bool dark) {
    return dark ? ImVec4(0.16f, 0.16f, 0.19f, 1.f) : ImVec4(0.97f, 0.97f, 0.99f, 1.f);
}

const char* DwmGlassModeName() { return g_dwmGlassMode; }

void ApplyTheme(const std::string& theme) {
    const std::string t = NormalizeTheme(theme);
    const bool dark = t == "Dark";
    ImGui::StyleColorsDark();
    if (!dark) ImGui::StyleColorsLight();
    ApplyModernFluentStyle(dark);
}

void SyncDwmTheme(HWND hwnd, const std::string& theme, bool /*perPixelAlphaSwapChain*/) {
    if (!hwnd) return;

    const std::string t = NormalizeTheme(theme);
    const bool dark = t == "Dark";

    SetDwmGlassMode(dark ? "dark chrome" : "light chrome");

    BOOL darkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

    int backdrop = DWMSBT_DISABLE;
    if (dark) backdrop = DWMSBT_TABBEDWINDOW;
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));

    const MARGINS margins{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    const int round = DWMWCP_ROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof(round));
}

void FrameClearColor(const std::string& theme, bool /*alphaSwapChain*/, float rgba[4]) {
    const bool dark = IsDarkTheme(theme);
    if (dark) {
        rgba[0] = 0.10f;
        rgba[1] = 0.10f;
        rgba[2] = 0.11f;
        rgba[3] = 1.f;
    } else {
        rgba[0] = 0.97f;
        rgba[1] = 0.97f;
        rgba[2] = 0.98f;
        rgba[3] = 1.f;
    }
}

} // namespace maku::ui
