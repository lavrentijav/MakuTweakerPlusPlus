#include "ui/Fonts.h"
#include "core/StringUtil.h"
#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace maku::ui {

static ImFont* g_fontUi = nullptr;
static ImFont* g_fontTitle = nullptr;

static const ImWchar kUiRanges[] = {
    0x0020, 0x00FF, // Basic Latin + Latin-1
    0x0400, 0x04FF, // Cyrillic
    0x0500, 0x052F, // Cyrillic supplement
    0x0100, 0x017F, // Latin extended
    0x1E00, 0x1EFF, // Vietnamese
    0x2000, 0x206F, // Punctuation
    0,
};

static std::wstring WindowsFontsDir() {
    wchar_t winDir[MAX_PATH]{};
    if (GetWindowsDirectoryW(winDir, MAX_PATH) == 0)
        return L"C:\\Windows\\Fonts";
    return std::wstring(winDir) + L"\\Fonts";
}

static std::string SystemFontPath(const wchar_t* fileName) {
    return util::ToUtf8(WindowsFontsDir() + L"\\" + fileName);
}

static bool FileExists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

static ImFont* LoadSegoe(ImGuiIO& io, const char* file, float size, ImFontConfig* cfg) {
    if (!FileExists(file)) return nullptr;
    return io.Fonts->AddFontFromFileTTF(file, size, cfg, kUiRanges);
}

static void MergeCjk(ImGuiIO& io, float size, ImFont* dst) {
    if (!dst) return;
    static const ImWchar cjk[] = {
        0x3000, 0x30FF, 0x4E00, 0x9FFF, 0xAC00, 0xD7A3, 0,
    };
    const wchar_t* files[] = {L"msyh.ttc", L"malgun.ttf", L"meiryo.ttc"};
    ImFontConfig m{};
    m.MergeMode = true;
    m.DstFont = dst;
    for (auto f : files) {
        const std::string p = SystemFontPath(f);
        if (FileExists(p) && io.Fonts->AddFontFromFileTTF(p.c_str(), size, &m, cjk))
            return;
    }
}

void InitFonts(HWND hwnd) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    float dpi = 1.f;
    if (hwnd)
        dpi = ImGui_ImplWin32_GetDpiScaleForHwnd(hwnd);

    const float uiSize = 15.f * dpi;
    const float titleSize = 22.f * dpi;

    ImFontConfig cfg{};
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH = true;

    const std::string segoeUi = SystemFontPath(L"segoeui.ttf");
    const std::string segoeBold = SystemFontPath(L"segoeuib.ttf");
    const std::string segoeSemibold = SystemFontPath(L"seguisb.ttf");

    g_fontUi = LoadSegoe(io, segoeUi.c_str(), uiSize, &cfg);
    if (!g_fontUi)
        g_fontUi = LoadSegoe(io, SystemFontPath(L"tahoma.ttf").c_str(), uiSize, &cfg);
    if (!g_fontUi) {
        g_fontUi = io.Fonts->AddFontDefault();
        ImFontConfig merge{};
        merge.MergeMode = true;
        merge.DstFont = g_fontUi;
        if (FileExists(segoeUi))
            io.Fonts->AddFontFromFileTTF(segoeUi.c_str(), uiSize, &merge,
                                         io.Fonts->GetGlyphRangesCyrillic());
    }

    MergeCjk(io, uiSize, g_fontUi);

    ImFontConfig cfgTitle = cfg;
    g_fontTitle = LoadSegoe(io, segoeBold.c_str(), titleSize, &cfgTitle);
    if (!g_fontTitle)
        g_fontTitle = LoadSegoe(io, segoeSemibold.c_str(), titleSize, &cfgTitle);
    if (!g_fontTitle)
        g_fontTitle = g_fontUi;

    io.FontDefault = g_fontUi;
}

void ReloadFonts(HWND hwnd) {
    InitFonts(hwnd);
    if (hwnd) {
        ImGui_ImplDX11_InvalidateDeviceObjects();
        ImGui_ImplDX11_CreateDeviceObjects();
    }
}

ImFont* FontUi() { return g_fontUi ? g_fontUi : ImGui::GetFont(); }

ImFont* FontTitle() { return g_fontTitle ? g_fontTitle : FontUi(); }

} // namespace maku::ui
