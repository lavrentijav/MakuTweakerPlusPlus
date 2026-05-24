#include "platform/MetricsTray.h"

#include "app/Branding.h"
#include "core/Localization.h"
#include "core/LogFile.h"
#include "core/MetricsDb.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include "platform/MetricsService.h"
#include "platform/TrayIconLoad.h"
#include <shellapi.h>
#include <userenv.h>
#include <vector>
#include <wtsapi32.h>

#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")

namespace maku::metrics_tray {
namespace {

constexpr UINT kTrayCallback = WM_USER + 42;
constexpr UINT kMsgOpenGui = WM_USER + 43;
constexpr UINT kCmdOpen = 1001;
constexpr UINT kCmdMonitor = 1002;
constexpr UINT kCmdStartSvc = 1003;
constexpr UINT kCmdStopSvc = 1004;
constexpr UINT kCmdOpenData = 1006;
constexpr UINT kCmdExitTray = 1005;

constexpr UINT_PTR kTimerRefresh = 1;
constexpr UINT kRefreshIntervalMs = 2000;
/// Allow the standalone tray to linger this long after the service stops before exiting;
/// gives the user time to restart it from the menu without losing the icon.
constexpr DWORD kStandaloneLingerMs = 5 * 60 * 1000;

constexpr wchar_t kTrayWindowClass[] = L"MakuTweakerMetricsTray";
constexpr wchar_t kTrayMutex[] = L"Global\\MakuTweakerMetricsTray";

struct LocalizedLabels {
    std::wstring open;
    std::wstring monitor;
    std::wstring startSvc;
    std::wstring stopSvc;
    std::wstring notInstalled;
    std::wstring exitTray;
    std::wstring openData;
};

struct TrayState {
    HWND hwnd = nullptr;
    HINSTANCE inst = nullptr;
    HANDLE mutex = nullptr;
    NOTIFYICONDATAW nid{};
    bool standalone = false;
    DWORD svcStoppedSinceTick = 0;
    UINT taskbarCreatedMsg = 0;
    std::string cachedLang;
    LocalizedLabels labels;
    metrics::MetricsDb db;
    bool dbReady = false;
    std::wstring lastTip;
    DWORD lastTipTick = 0;
};

TrayState& State() {
    static TrayState s;
    return s;
}

void RefreshLabels() {
    auto& st = State();
    Settings s;
    s.Load();
    if (st.cachedLang == s.lang && !st.labels.open.empty()) return;
    st.cachedLang = s.lang;
    l10n::Localization l(s.lang);
    st.labels.open = util::ToWide(l.Get("mon", "tray", "open"));
    st.labels.monitor = util::ToWide(l.Get("mon", "tray", "monitor"));
    st.labels.startSvc = util::ToWide(l.Get("mon", "tray", "start_service"));
    st.labels.stopSvc = util::ToWide(l.Get("mon", "tray", "stop_service"));
    st.labels.notInstalled = util::ToWide(l.Get("mon", "tray", "not_installed"));
    st.labels.exitTray = util::ToWide(l.Get("mon", "tray", "exit_tray"));
    const std::string openData = l.Get("mon", "tray", "open_data");
    st.labels.openData = util::ToWide(openData.empty() ? std::string("Open data folder") : openData);
}

std::wstring BuildTooltip() {
    auto& st = State();
    wchar_t status[64]{};
    metrics_svc::GetStatus(status, 64);

    metrics::SystemSample latest{};
    if (st.dbReady) latest = st.db.QueryLatestSystem();

    wchar_t buf[128];
    if (latest.ts > 0) {
        swprintf_s(buf, L"%s | %ls\nCPU %d%%  RAM %d%%", brand::kDisplayName, status,
                   static_cast<int>(latest.cpuTotal), static_cast<int>(latest.ramPct));
    } else {
        swprintf_s(buf, L"%s | %ls", brand::kDisplayName, status);
    }
    return buf;
}

void UpdateTrayTip() {
    auto& st = State();
    if (!st.nid.hWnd) return;
    const std::wstring tip = BuildTooltip();
    const DWORD now = GetTickCount();
    if (tip == st.lastTip && now - st.lastTipTick < 10000) return;
    st.lastTip = tip;
    st.lastTipTick = now;
    wcsncpy_s(st.nid.szTip, tip.c_str(), _TRUNCATE);
    st.nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &st.nid);
}

bool CreateTrayIcon(HWND hwnd, HINSTANCE inst, bool standalone) {
    auto& st = State();
    st.hwnd = hwnd;
    st.inst = inst;
    st.standalone = standalone;
    if (standalone && !metrics_svc::IsRunning()) st.svcStoppedSinceTick = GetTickCount();
    st.nid = {};
    st.nid.cbSize = sizeof(NOTIFYICONDATAW);
    st.nid.hWnd = hwnd;
    st.nid.uID = standalone ? 2u : 1u;
    st.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    st.nid.uCallbackMessage = kTrayCallback;
    st.nid.hIcon = platform::tray_icon::LoadNotifyAreaIcon(inst);
    const std::wstring tip = BuildTooltip();
    wcsncpy_s(st.nid.szTip, tip.c_str(), _TRUNCATE);
    st.lastTip = tip;
    st.lastTipTick = GetTickCount();
    if (Shell_NotifyIconW(NIM_ADD, &st.nid) == FALSE) return false;
    platform::tray_icon::SetNotifyIconVersion(st.nid);
    return true;
}

bool RecreateTrayIcon() {
    auto& st = State();
    if (!st.hwnd || !st.inst) return false;
    if (st.nid.hIcon) DestroyIcon(st.nid.hIcon);
    st.nid.hIcon = nullptr;
    return CreateTrayIcon(st.hwnd, st.inst, st.standalone);
}

void DestroyTrayIcon() {
    auto& st = State();
    if (st.nid.hWnd) Shell_NotifyIconW(NIM_DELETE, &st.nid);
    if (st.nid.hIcon) DestroyIcon(st.nid.hIcon);
    st.nid = {};
}

HWND FindMainWindow() { return FindWindowW(brand::kWindowClass, nullptr); }

HWND FindTrayWindow() { return FindWindowW(kTrayWindowClass, brand::kDisplayName); }

void OpenMainGui() {
    if (IsMainGuiAlive()) {
        const HWND main = FindMainWindow();
        ShowWindow(main, SW_RESTORE);
        SetForegroundWindow(main);
        return;
    }
    LaunchForegroundGui(L"--foreground-gui");
}

void OpenDataFolder() {
    const std::wstring path = util::GetSharedDataPath();
    if (path.empty()) return;
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void ShowTrayMenu(HWND hwnd) {
    RefreshLabels();
    auto& st = State();

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, kCmdOpen, st.labels.open.c_str());
    AppendMenuW(menu, MF_STRING, kCmdMonitor, st.labels.monitor.c_str());
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    if (metrics_svc::IsRunning())
        AppendMenuW(menu, MF_STRING, kCmdStopSvc, st.labels.stopSvc.c_str());
    else if (metrics_svc::IsInstalled())
        AppendMenuW(menu, MF_STRING, kCmdStartSvc, st.labels.startSvc.c_str());
    else
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, st.labels.notInstalled.c_str());

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdOpenData, st.labels.openData.c_str());

    if (st.standalone) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kCmdExitTray, st.labels.exitTray.c_str());
    }

    SetForegroundWindow(hwnd);
    POINT pt{};
    GetCursorPos(&pt);
    const UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y,
                                    0, hwnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(hwnd, WM_NULL, 0, 0);

    switch (cmd) {
    case kCmdOpen:
        OpenMainGui();
        break;
    case kCmdMonitor:
        LaunchForegroundGui(L"--foreground-gui --tab mon");
        break;
    case kCmdStartSvc:
        if (metrics_svc::Start()) State().svcStoppedSinceTick = 0;
        UpdateTrayTip();
        break;
    case kCmdStopSvc:
        if (metrics_svc::Stop()) State().svcStoppedSinceTick = GetTickCount();
        UpdateTrayTip();
        break;
    case kCmdOpenData:
        OpenDataFolder();
        break;
    case kCmdExitTray:
        QuitApplication();
        break;
    default:
        break;
    }
}

bool ProcessTrayCallback(HWND hwnd, WPARAM, LPARAM lp) {
    switch (LOWORD(lp)) {
    case WM_CONTEXTMENU:
    case WM_RBUTTONUP:
    case WM_RBUTTONDOWN:
        ShowTrayMenu(hwnd);
        return true;
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_USER: // NIN_SELECT (NOTIFYICON_VERSION_4)
        OpenMainGui();
        return true;
    default:
        return false;
    }
}

void OnRefreshTimer(HWND /*hwnd*/) {
    auto& st = State();

    UpdateTrayTip();

    if (!st.standalone) return;

    if (metrics_svc::IsRunning()) {
        st.svcStoppedSinceTick = 0;
        return;
    }

    if (!metrics_svc::IsInstalled()) {
        logf::Info("Metrics tray: service uninstalled, exiting");
        PostMessageW(st.hwnd, WM_CLOSE, 0, 0);
        return;
    }

    if (st.svcStoppedSinceTick == 0) {
        st.svcStoppedSinceTick = GetTickCount();
        return;
    }
    if (GetTickCount() - st.svcStoppedSinceTick > kStandaloneLingerMs) {
        logf::Info("Metrics tray: service stopped, linger expired, exiting");
        PostMessageW(st.hwnd, WM_CLOSE, 0, 0);
    }
}

LRESULT CALLBACK TrayWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto& st = State();
    if (msg == st.taskbarCreatedMsg && st.taskbarCreatedMsg != 0) {
        RecreateTrayIcon();
        return 0;
    }
    switch (msg) {
    case WM_CREATE:
        return 0;
    case kTrayCallback:
        ProcessTrayCallback(hwnd, wp, lp);
        return 0;
    case WM_TIMER:
        if (wp == kTimerRefresh) OnRefreshTimer(hwnd);
        return 0;
    case kMsgOpenGui:
        OpenMainGui();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kTimerRefresh);
        DestroyTrayIcon();
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace

bool IsMainGuiAlive() {
    const HWND w = FindWindowW(brand::kWindowClass, nullptr);
    if (!w || !IsWindow(w)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (!pid) return false;
    HANDLE proc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!proc) return false;
    DWORD exitCode = STILL_ACTIVE;
    const bool alive =
        GetExitCodeProcess(proc, &exitCode) != FALSE && exitCode == STILL_ACTIVE;
    CloseHandle(proc);
    return alive;
}

void LaunchForegroundGui(const std::wstring& args) {
    const std::wstring exe = util::GetExePath();
    ShellExecuteW(nullptr, L"open", exe.c_str(), args.c_str(), util::GetExeDirectory().c_str(),
                  SW_SHOW);
}

void EnsureStandaloneTray() {
    if (FindTrayWindow() || !metrics_svc::IsRunning()) return;
    LaunchInUserSession();
}

void QuitApplication() {
    if (HWND main = FindMainWindow()) PostMessageW(main, kMsgForceQuitMain, 0, 0);
    if (HWND tray = FindTrayWindow()) PostMessageW(tray, WM_CLOSE, 0, 0);
}

void RequestOpenMainGui(const std::wstring& extraArgs) {
    if (IsMainGuiAlive()) {
        const HWND main = FindMainWindow();
        ShowWindow(main, SW_RESTORE);
        SetForegroundWindow(main);
        return;
    }
    EnsureStandaloneTray();
    const std::wstring args = extraArgs.empty() ? L"--foreground-gui" : extraArgs;
    LaunchForegroundGui(args);
}

bool IsForegroundGuiProcess() {
    const wchar_t* cmd = GetCommandLineW();
    return cmd && wcsstr(cmd, L"--foreground-gui") != nullptr;
}

bool ProcessMainTrayMessage(HWND hwnd, WPARAM wp, LPARAM lp) {
    return ProcessTrayCallback(hwnd, wp, lp);
}

void ShutdownStandalone() {
    if (HWND tray = FindTrayWindow()) PostMessageW(tray, WM_CLOSE, 0, 0);
}

namespace {
bool LaunchTrayProcess(HANDLE userToken) {
    wchar_t cmdLine[1024];
    swprintf_s(cmdLine, L"\"%ls\" --metrics-tray", util::GetExePath().c_str());
    std::vector<wchar_t> cmdBuf(cmdLine, cmdLine + wcslen(cmdLine) + 1);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.lpDesktop = const_cast<wchar_t*>(L"winsta0\\default");
    PROCESS_INFORMATION pi{};

    auto finish = [&](BOOL ok) -> bool {
        if (!ok) {
            if (logf::Active()) {
                logf::Error("LaunchTrayProcess failed: Win32=" +
                            std::to_string(GetLastError()));
            }
            return false;
        }
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    };

    if (!userToken) {
        return finish(CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                                    CREATE_NO_WINDOW, nullptr, util::GetExeDirectory().c_str(),
                                    &si, &pi));
    }

    HANDLE primaryToken = nullptr;
    if (!DuplicateTokenEx(userToken, MAXIMUM_ALLOWED, nullptr, SecurityImpersonation,
                          TokenPrimary, &primaryToken))
        return false;

    LPVOID env = nullptr;
    CreateEnvironmentBlock(&env, primaryToken, FALSE);
    const BOOL ok = CreateProcessAsUserW(
        primaryToken, nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW, env, util::GetExeDirectory().c_str(), &si,
        &pi);
    if (env) DestroyEnvironmentBlock(env);
    CloseHandle(primaryToken);
    return finish(ok);
}
} // namespace

void LaunchInUserSession() {
    if (FindTrayWindow()) return;

    const DWORD sessionId = WTSGetActiveConsoleSessionId();
    if (sessionId != 0xFFFFFFFF && sessionId != 0) {
        HANDLE token = nullptr;
        if (WTSQueryUserToken(sessionId, &token)) {
            if (LaunchTrayProcess(token)) {
                CloseHandle(token);
                return;
            }
            if (logf::Active()) {
                logf::Warn("WTSQueryUserToken ok but CreateProcessAsUser failed for metrics tray");
            }
            CloseHandle(token);
        } else if (logf::Active()) {
            logf::Warn("WTSQueryUserToken failed for session " + std::to_string(sessionId) +
                       ": Win32=" + std::to_string(GetLastError()));
        }
    }

    LaunchTrayProcess(nullptr);
}

int RunStandalone(HINSTANCE inst, bool openGuiOnStart) {
    auto& st = State();
    st.mutex = CreateMutexW(nullptr, TRUE, kTrayMutex);
    if (!st.mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (st.mutex) CloseHandle(st.mutex);
        st.mutex = nullptr;
        return 0;
    }

    logf::InitDefault();

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = TrayWndProc;
    wc.hInstance = inst;
    wc.lpszClassName = kTrayWindowClass;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;

    HWND hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kTrayWindowClass, brand::kDisplayName,
                                WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, inst, nullptr);
    if (!hwnd) return 1;

    st.taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    ChangeWindowMessageFilterEx(hwnd, st.taskbarCreatedMsg, MSGFLT_ALLOW, nullptr);

    st.dbReady = st.db.Open(st.db.DefaultPath());
    RefreshLabels();

    if (!CreateTrayIcon(hwnd, inst, true)) {
        if (st.dbReady) st.db.Close();
        return 1;
    }
    SetTimer(hwnd, kTimerRefresh, kRefreshIntervalMs, nullptr);
    if (openGuiOnStart) PostMessageW(hwnd, kMsgOpenGui, 0, 0);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (st.dbReady) {
        st.db.Close();
        st.dbReady = false;
    }
    if (st.mutex) {
        CloseHandle(st.mutex);
        st.mutex = nullptr;
    }
    return static_cast<int>(msg.wParam);
}

} // namespace maku::metrics_tray
