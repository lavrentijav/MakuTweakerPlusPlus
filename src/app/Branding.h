#pragma once

namespace maku::brand {

/// User-visible product name (window title, tray, messages).
inline constexpr wchar_t kDisplayName[] = L"MakuTweaker++";
inline constexpr char kDisplayNameUtf8[] = "MakuTweaker++";

/// Safe folder names (no '+' in paths).
inline constexpr wchar_t kAppDataFolder[] = L"MakuTweakerPlusPlus";
inline constexpr wchar_t kWindowClass[] = L"MakuTweakerPlusPlus";

/// Resource ID for AppIcon.rc (must match cmake/AppIcon.rc.in).
inline constexpr int kAppIconResourceId = 101;

} // namespace maku::brand
