#include "ui/Theme.h"
#include <imgui.h>
#include <dwmapi.h>

namespace maku::ui {

static void SetFluentColors(bool dark) {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6.f;
    s.FrameRounding = 4.f;
    s.ScrollbarSize = 12.f;
    s.FramePadding = ImVec2(8.f, 5.f);
    s.ItemSpacing = ImVec2(8.f, 6.f);
    ImVec4* c = s.Colors;
    if (dark) {
        c[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.12f, 0.94f);
        c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.09f, 0.10f, 1.f);
        c[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.f);
        c[ImGuiCol_Button] = ImVec4(0.26f, 0.26f, 0.30f, 1.f);
        c[ImGuiCol_Header] = ImVec4(0.26f, 0.40f, 0.70f, 0.55f);
        c[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.97f, 1.f);
    } else {
        c[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.97f, 0.94f);
        c[ImGuiCol_ChildBg] = ImVec4(0.98f, 0.98f, 0.99f, 1.f);
        c[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.90f, 0.92f, 1.f);
        c[ImGuiCol_Button] = ImVec4(0.85f, 0.85f, 0.88f, 1.f);
        c[ImGuiCol_Header] = ImVec4(0.26f, 0.40f, 0.70f, 0.35f);
        c[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.12f, 1.f);
    }
}

void ApplyTheme(const std::string& theme) {
    bool dark = theme == "Dark";
    ImGui::StyleColorsDark();
    if (!dark) ImGui::StyleColorsLight();
    SetFluentColors(dark);
}

void SyncDwmDark(HWND hwnd, bool dark) {
    BOOL v = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, 20, &v, sizeof(v));
}

} // namespace maku::ui
