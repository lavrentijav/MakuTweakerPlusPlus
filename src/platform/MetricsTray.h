#pragma once
#include <string>
#include <windows.h>

namespace maku::metrics_tray {

/// Posted to the main GUI window to exit instead of hiding to tray.
inline constexpr UINT kMsgForceQuitMain = WM_APP + 7;

bool IsMainGuiAlive();

/// Handle tray notify message for the main GUI window (right-click menu).
bool ProcessMainTrayMessage(HWND hwnd, WPARAM wp, LPARAM lp);

/// Hidden-window tray-only mode while the metrics service runs without GUI.
int RunStandalone(HINSTANCE inst, bool openGuiOnStart = false);

/// Start tray helper in the active user session (from service or installer).
void LaunchInUserSession();

/// Close standalone tray if running (before showing main GUI).
void ShutdownStandalone();

/// Focus an existing GUI or spawn a foreground GUI instance (separate process).
void RequestOpenMainGui(const std::wstring& extraArgs = L"--foreground-gui");

/// True when this process was started with --foreground-gui (detached GUI while service runs).
bool IsForegroundGuiProcess();

/// Launch GUI in the user session (skips service duplicate-instance handoff).
void LaunchForegroundGui(const std::wstring& args);

/// Ensure metrics tray helper is running (when service collects in background).
void EnsureStandaloneTray();

/// Quit tray and signal the main window to exit.
void QuitApplication();

} // namespace maku::metrics_tray
