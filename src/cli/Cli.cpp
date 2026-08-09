#include "cli/Cli.h"

#include "app/Branding.h"
#include "app/Version.h"
#include "core/Analytics.h"
#include "core/Benchmark.h"
#include "core/DisallowRun.h"
#include "core/HostsBan.h"
#include "core/Localization.h"
#include "core/OsUtil.h"
#include "core/PresetIO.h"
#include "core/ProcessMgrUtil.h"
#include "core/ProcessRunner.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include "core/TweakRegistry.h"
#include "core/UwpUtil.h"
#include "core/Wmi.h"

#include <nlohmann/json.hpp>

#include <windows.h>

#include <fcntl.h>
#include <io.h>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace maku::cli {
namespace {

using json = nlohmann::json;

// --- console plumbing -------------------------------------------------------

bool g_ownsConsole = false;

/// Points a CRT stream at the handle the launcher gave us, falling back to the
/// console device. Honouring an inherited pipe or file is what makes
/// `MakuTweaker++.exe tweak list > out.txt` and shell capture work at all —
/// freopen("CONOUT$") unconditionally would send output past the redirection.
void BindStream(HANDLE inherited, DWORD stdHandleId, FILE* stream, const char* device,
                const char* mode) {
    if (inherited && inherited != INVALID_HANDLE_VALUE) {
        const int flags = (stdHandleId == STD_INPUT_HANDLE) ? _O_RDONLY : _O_APPEND;
        const int fd = _open_osfhandle(reinterpret_cast<intptr_t>(inherited), flags);
        if (fd >= 0) {
            if (_dup2(fd, _fileno(stream)) == 0) {
                setvbuf(stream, nullptr, _IONBF, 0);
                return;
            }
            _close(fd);
        }
    }
    FILE* reopened = nullptr;
    freopen_s(&reopened, device, mode, stream);
}

/// The binary is linked /SUBSYSTEM:WINDOWS so it has no console of its own.
/// Attach to the invoking shell when there is one; otherwise open a window so
/// double-clicking `MakuTweaker++.exe help` still shows something.
void EnsureConsole() {
    // Read the launcher's handles first: AttachConsole/AllocConsole replace the
    // std handles with console ones, which would drop any redirection.
    const HANDLE inheritedOut = GetStdHandle(STD_OUTPUT_HANDLE);
    const HANDLE inheritedErr = GetStdHandle(STD_ERROR_HANDLE);
    const HANDLE inheritedIn = GetStdHandle(STD_INPUT_HANDLE);
    const bool redirected = inheritedOut && inheritedOut != INVALID_HANDLE_VALUE;

    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        g_ownsConsole = false;
    } else if (!redirected && AllocConsole()) {
        g_ownsConsole = true;
    }

    BindStream(inheritedOut, STD_OUTPUT_HANDLE, stdout, "CONOUT$", "w");
    BindStream(inheritedErr, STD_ERROR_HANDLE, stderr, "CONOUT$", "w");
    BindStream(inheritedIn, STD_INPUT_HANDLE, stdin, "CONIN$", "r");

    // Localized tweak names are UTF-8; without this they print as mojibake.
    SetConsoleOutputCP(CP_UTF8);
    std::ios::sync_with_stdio(true);
}

void ReleaseConsole() {
    if (!g_ownsConsole) return;
    std::cout << "\nPress Enter to close..." << std::flush;
    std::cin.get();
}

void Out(const std::string& s) { std::cout << s << "\n"; }
void Err(const std::string& s) { std::cerr << s << "\n"; }

// --- argument helpers -------------------------------------------------------

struct Args {
    std::vector<std::string> positional;
    std::map<std::string, std::string> flags; ///< "--json" -> "" or "--page" -> "exp"
};

Args ParseArgs(int argc, wchar_t** argv, int startIndex) {
    Args a;
    for (int i = startIndex; i < argc; ++i) {
        std::string arg = util::ToUtf8(argv[i]);
        if (arg.rfind("--", 0) != 0) {
            a.positional.push_back(std::move(arg));
            continue;
        }
        const size_t eq = arg.find('=');
        if (eq != std::string::npos) {
            a.flags[arg.substr(0, eq)] = arg.substr(eq + 1);
            continue;
        }
        // "--page exp" form: consume the next token when it is not a flag.
        if (i + 1 < argc) {
            const std::string next = util::ToUtf8(argv[i + 1]);
            if (next.rfind("--", 0) != 0 && arg != "--json" && arg != "--yes" && arg != "--all" &&
                arg != "--multi" && arg != "--single") {
                a.flags[arg] = next;
                ++i;
                continue;
            }
        }
        a.flags[arg] = "";
    }
    return a;
}

bool HasFlag(const Args& a, const char* name) { return a.flags.count(name) != 0; }

std::string FlagValue(const Args& a, const char* name, const std::string& fallback = {}) {
    auto it = a.flags.find(name);
    return it == a.flags.end() || it->second.empty() ? fallback : it->second;
}

bool ParseBool(const std::string& s, bool& out) {
    std::string v = s;
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (v == "on" || v == "1" || v == "true" || v == "yes" || v == "enable") {
        out = true;
        return true;
    }
    if (v == "off" || v == "0" || v == "false" || v == "no" || v == "disable") {
        out = false;
        return true;
    }
    return false;
}

/// Loads the user's language so `tweak list` prints the same labels as the GUI.
const l10n::Localization& Loc() {
    static l10n::Localization* loc = [] {
        Settings s;
        s.Load();
        return new l10n::Localization(s.lang);
    }();
    return *loc;
}

int RequireAdmin(const char* what) {
    if (os::HasAdminRights()) return 0;
    Err(std::string("error: ") + what + " requires administrator rights; "
        "run this command from an elevated terminal");
    return 1;
}

// --- help -------------------------------------------------------------------

void PrintUsage() {
    Out(std::string(brand::kDisplayNameUtf8) + " " + version::kText + " - command line interface");
    Out("");
    Out("USAGE");
    Out("  MakuTweaker++.exe <command> [subcommand] [arguments] [--flags]");
    Out("  MakuTweaker++.exe                       launch the graphical interface");
    Out("");
    Out("TWEAKS");
    Out("  tweak list [--page <tag>] [--json]      list every tweak and its state");
    Out("  tweak pages                             list page tags usable with --page");
    Out("  tweak get <id> [--json]                 print one tweak's state");
    Out("  tweak set <id> <on|off>                 apply or revert a toggle");
    Out("  tweak run <id>                          run a one-shot action");
    Out("");
    Out("PRESETS");
    Out("  preset quick [--json]                   list the Quick Setup tweak ids");
    Out("  preset apply <id,id,...>                apply several tweaks in order");
    Out("  preset export <file>                    write app settings to a .mktw file");
    Out("  preset import <file>                    load app settings from a .mktw file");
    Out("");
    Out("WINDOWS UPDATE");
    Out("  wu status                               show the current feature-update pin");
    Out("  wu pin <release>                        pin to 22H2, 24H2, ...");
    Out("  wu unpin                                remove the pin");
    Out("");
    Out("PERFORMANCE");
    Out("  cpu throttle [<1-100>]                  read or set the max processor state");
    Out("  bench [--multi|--single] [--seconds N] [--json]");
    Out("");
    Out("SYSTEM");
    Out("  sysinfo [--json]                        CPU / RAM / OS summary");
    Out("  proc list [--filter <text>] [--json]    running processes");
    Out("  proc kill <pid|name>                    terminate by pid or image name");
    Out("  svc list [--filter <text>] [--json]     services");
    Out("  uwp list [--json]                       installed UWP packages");
    Out("  uwp remove <full-name> [...] --yes      remove UWP packages");
    Out("  shutdown <minutes> | shutdown cancel    schedule or cancel a shutdown");
    Out("");
    Out("BLOCKING");
    Out("  hosts list                              domains blocked via the hosts file");
    Out("  hosts ban <domain> [...]                add domains to the block list");
    Out("  hosts unban <domain> [...]              remove domains");
    Out("  hosts clear                             remove every managed entry");
    Out("  apps block <exe> [...]                  block executables (DisallowRun policy)");
    Out("  apps unblock                            clear the executable block list");
    Out("");
    Out("ANALYTICS");
    Out("  analytics status [--json]                whether sharing is on, plus your top tabs");
    Out("  analytics on | off                       answer the sharing question");
    Out("  analytics reset                          ask again on next GUI launch");
    Out("");
    Out("SETTINGS");
    Out("  settings list [--json]                  print the app's own settings");
    Out("  settings get <key>");
    Out("  settings set <key> <value>");
    Out("");
    Out("OTHER");
    Out("  version [--json]                        version and build number");
    Out("  help                                    this text");
    Out("");
    Out("EXIT CODES  0 ok, 1 failure, 2 usage error");
}

// --- tweak commands ---------------------------------------------------------

const char* KindName(tweak::Kind k) { return k == tweak::Kind::Toggle ? "toggle" : "action"; }

json TweakToJson(const tweak::Tweak& t, bool includeState) {
    json j;
    j["id"] = t.id;
    j["page"] = t.page;
    j["kind"] = KindName(t.kind);
    j["name"] = tweak::DisplayName(t, Loc());
    j["summary"] = t.summary;
    j["needsAdmin"] = t.needsAdmin;
    j["reboot"] = static_cast<int>(t.reboot);
    j["available"] = tweak::IsAvailable(t);
    if (includeState && t.kind == tweak::Kind::Toggle) j["state"] = tweak::Probe(t) ? "on" : "off";
    return j;
}

int CmdTweak(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: tweak <list|pages|get|set|run> ...");
        return 2;
    }
    const std::string& sub = a.positional[0];
    const bool asJson = HasFlag(a, "--json");

    if (sub == "pages") {
        std::vector<std::string> seen;
        for (const auto& t : tweak::All())
            if (std::find(seen.begin(), seen.end(), t.page) == seen.end()) seen.push_back(t.page);
        if (asJson) {
            Out(json(seen).dump(2));
        } else {
            for (const auto& p : seen)
                Out("  " + p + "  (" + std::to_string(tweak::ForPage(p).size()) + " tweaks)");
        }
        return 0;
    }

    if (sub == "list") {
        const std::string page = FlagValue(a, "--page");
        json arr = json::array();
        for (const auto& t : tweak::All()) {
            if (!page.empty() && page != t.page) continue;
            if (!tweak::IsAvailable(t)) continue;
            if (asJson) {
                arr.push_back(TweakToJson(t, true));
                continue;
            }
            std::string state = "-";
            if (t.kind == tweak::Kind::Toggle) state = tweak::Probe(t) ? "on" : "off";
            char line[512];
            snprintf(line, sizeof(line), "  %-4s %-34s %s", state.c_str(), t.id,
                     tweak::DisplayName(t, Loc()).c_str());
            Out(line);
        }
        if (asJson) Out(arr.dump(2));
        return 0;
    }

    if (a.positional.size() < 2) {
        Err("usage: tweak " + sub + " <id> ...");
        return 2;
    }
    const tweak::Tweak* t = tweak::Find(a.positional[1]);
    if (!t) {
        Err("error: unknown tweak id '" + a.positional[1] + "' (try: tweak list)");
        return 2;
    }
    if (!tweak::IsAvailable(*t)) {
        Err("error: '" + a.positional[1] + "' is not available on Windows build " +
            std::to_string(os::GetWindowsBuild()));
        return 1;
    }

    if (sub == "get") {
        if (asJson) {
            Out(TweakToJson(*t, true).dump(2));
        } else if (t->kind == tweak::Kind::Toggle) {
            Out(tweak::Probe(*t) ? "on" : "off");
        } else {
            Out("action");
        }
        return 0;
    }

    if (sub == "set") {
        if (t->kind != tweak::Kind::Toggle) {
            Err("error: '" + a.positional[1] + "' is an action; use `tweak run`");
            return 2;
        }
        if (a.positional.size() < 3) {
            Err("usage: tweak set <id> <on|off>");
            return 2;
        }
        bool on = false;
        if (!ParseBool(a.positional[2], on)) {
            Err("error: expected on or off, got '" + a.positional[2] + "'");
            return 2;
        }
        if (t->needsAdmin && RequireAdmin(t->id) != 0) return 1;
        if (!tweak::Apply(*t, on)) {
            Err("error: applying '" + a.positional[1] + "' failed");
            return 1;
        }
        Out(std::string(t->id) + " -> " + (on ? "on" : "off"));
        if (t->reboot == tweak::Reboot::Restart) Out("note: a reboot is required");
        if (t->reboot == tweak::Reboot::ExplorerRestart) Out("note: restart Explorer to see this");
        return 0;
    }

    if (sub == "run") {
        if (t->kind != tweak::Kind::Action) {
            Err("error: '" + a.positional[1] + "' is a toggle; use `tweak set`");
            return 2;
        }
        if (t->needsAdmin && RequireAdmin(t->id) != 0) return 1;
        if (!tweak::Apply(*t, true)) {
            Err("error: running '" + a.positional[1] + "' failed");
            return 1;
        }
        Out(std::string(t->id) + " done");
        return 0;
    }

    Err("error: unknown subcommand 'tweak " + sub + "'");
    return 2;
}

// --- presets ----------------------------------------------------------------

/// Ids the GUI's Quick Setup page offers, kept here so both surfaces agree.
/// (ui/TweakPage.cpp owns the canonical list; this mirrors it for the CLI,
/// which must not link UI code.)
const std::vector<std::string>& QuickIds() {
    static const std::vector<std::string> ids = {
        "explorer.show-hidden",     "explorer.show-extensions", "explorer.open-this-pc",
        "explorer.show-this-pc-desktop", "explorer.clean-shortcut-name",
        "explorer.hide-gallery",    "explorer.fix-delegate-folders",
        "per.hide-taskbar-widgets", "per.no-search-highlights", "per.clipboard-history",
        "per.fast-context-menu",    "per.old-context-menu",     "per.end-task",
        "per.verbose-boot",         "sys.bing-search-off",      "sys.sticky-keys-off",
        "sys.chkdsk-timeout",       "sys.block-bitlocker",      "compon.directplay",
        "wu.block-updates",         "sys.core-isolation-off",   "sys.uac-off",
        "sys.smartscreen-off",      "sys.hibernate-off",        "sys.telemetry-off",
        "adv.vbs-off",
    };
    return ids;
}

std::vector<std::string> SplitCsv(const std::string& s) {
    std::vector<std::string> out;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t comma = s.find(',', start);
        std::string piece = s.substr(start, comma == std::string::npos ? comma : comma - start);
        // Trim so "a, b" works as well as "a,b".
        const size_t b = piece.find_first_not_of(" \t");
        const size_t e = piece.find_last_not_of(" \t");
        if (b != std::string::npos) out.push_back(piece.substr(b, e - b + 1));
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return out;
}

int CmdPreset(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: preset <quick|apply|export|import> ...");
        return 2;
    }
    const std::string& sub = a.positional[0];

    if (sub == "quick") {
        if (HasFlag(a, "--json")) {
            Out(json(QuickIds()).dump(2));
            return 0;
        }
        for (const auto& id : QuickIds()) {
            const tweak::Tweak* t = tweak::Find(id);
            if (t && tweak::IsAvailable(*t)) Out("  " + id + "  " + tweak::DisplayName(*t, Loc()));
        }
        return 0;
    }

    if (sub == "apply") {
        std::vector<std::string> ids;
        if (a.positional.size() >= 2 && a.positional[1] != "quick")
            ids = SplitCsv(a.positional[1]);
        else
            ids = QuickIds();

        int failures = 0, applied = 0;
        for (const auto& id : ids) {
            const tweak::Tweak* t = tweak::Find(id);
            if (!t) {
                Err("  skip  " + id + " (unknown id)");
                ++failures;
                continue;
            }
            if (!tweak::IsAvailable(*t)) {
                Out("  skip  " + id + " (not available on this build)");
                continue;
            }
            if (t->needsAdmin && !os::HasAdminRights()) {
                Err("  skip  " + id + " (needs administrator)");
                ++failures;
                continue;
            }
            if (tweak::Apply(*t, true)) {
                Out("  ok    " + id);
                ++applied;
            } else {
                Err("  fail  " + id);
                ++failures;
            }
        }
        Out("applied " + std::to_string(applied) + ", failed " + std::to_string(failures));
        return failures == 0 ? 0 : 1;
    }

    if (sub == "export" || sub == "import") {
        if (a.positional.size() < 2) {
            Err("usage: preset " + sub + " <file>");
            return 2;
        }
        const std::wstring path = util::ToWide(a.positional[1]);
        std::wstring error;
        const bool ok = sub == "export" ? preset::ExportSettings(path, &error)
                                        : preset::ImportSettings(path, &error);
        if (!ok) {
            Err("error: " + util::ToUtf8(error));
            return 1;
        }
        Out(sub == "export" ? "exported" : "imported");
        return 0;
    }

    Err("error: unknown subcommand 'preset " + sub + "'");
    return 2;
}

// --- windows update ---------------------------------------------------------

int CmdWu(const Args& a) {
    const std::string sub = a.positional.empty() ? "status" : a.positional[0];

    if (sub == "status") {
        const std::string pin = tweak::GetTargetRelease();
        if (HasFlag(a, "--json")) {
            json j;
            j["pinned"] = !pin.empty();
            j["release"] = pin;
            j["options"] = tweak::TargetReleaseOptions();
            Out(j.dump(2));
        } else if (pin.empty()) {
            Out("not pinned");
        } else {
            Out("pinned to " + pin);
        }
        return 0;
    }

    if (sub == "pin") {
        if (a.positional.size() < 2) {
            Err("usage: wu pin <release>");
            return 2;
        }
        const auto& options = tweak::TargetReleaseOptions();
        if (std::find(options.begin(), options.end(), a.positional[1]) == options.end()) {
            std::string list;
            for (const auto& o : options) list += (list.empty() ? "" : ", ") + o;
            Err("error: unknown release '" + a.positional[1] + "'; expected one of " + list);
            return 2;
        }
        if (RequireAdmin("pinning the feature update") != 0) return 1;
        tweak::SetTargetRelease(a.positional[1]);
        Out("pinned to " + a.positional[1] + " (reboot recommended)");
        return 0;
    }

    if (sub == "unpin") {
        if (RequireAdmin("removing the feature-update pin") != 0) return 1;
        tweak::ClearTargetRelease();
        Out("pin removed");
        return 0;
    }

    Err("error: unknown subcommand 'wu " + sub + "'");
    return 2;
}

// --- performance ------------------------------------------------------------

int CmdCpu(const Args& a) {
    if (a.positional.empty() || a.positional[0] != "throttle") {
        Err("usage: cpu throttle [<1-100>]");
        return 2;
    }
    if (a.positional.size() < 2) {
        const int value = tweak::GetCpuThrottleMax();
        if (value < 0) {
            Err("error: could not read the current processor state limit");
            return 1;
        }
        Out(std::to_string(value) + "%");
        return 0;
    }
    const int percent = std::atoi(a.positional[1].c_str());
    if (percent < 1 || percent > 100) {
        Err("error: expected a percentage between 1 and 100");
        return 2;
    }
    if (RequireAdmin("changing the processor state limit") != 0) return 1;
    std::string error;
    if (!tweak::SetCpuThrottleMax(percent, &error)) {
        Err("error: " + (error.empty() ? std::string("powercfg failed") : error));
        return 1;
    }
    Out("max processor state -> " + std::to_string(percent) + "%");
    return 0;
}

int CmdBench(const Args& a) {
    const bool multi = HasFlag(a, "--multi") || !HasFlag(a, "--single");
    int seconds = std::atoi(FlagValue(a, "--seconds", "10").c_str());
    if (seconds < 1) seconds = 1;
    if (seconds > 120) seconds = 120;

    if (!HasFlag(a, "--json"))
        Out(std::string("running ") + (multi ? "multi" : "single") + "-thread benchmark for " +
            std::to_string(seconds) + "s...");

    const auto result = bench::Run(multi, seconds * 1000);
    const std::wstring cpu = wmi::QueryScalar(L"SELECT Name FROM Win32_Processor", L"Name");

    if (HasFlag(a, "--json")) {
        json j;
        j["cpu"] = util::ToUtf8(cpu);
        j["mode"] = multi ? "multi" : "single";
        j["score"] = result.score;
        j["totalOps"] = result.totalOps;
        j["elapsedMs"] = result.elapsedMs;
        Out(j.dump(2));
    } else {
        Out("cpu    " + util::ToUtf8(cpu));
        Out("score  " + std::to_string(static_cast<long long>(result.score)));
        Out("ops    " + std::to_string(result.totalOps));
        Out("time   " + std::to_string(result.elapsedMs) + " ms");
    }
    return 0;
}

// --- system inventory -------------------------------------------------------

int CmdSysInfo(const Args& a) {
    MEMORYSTATUSEX mem{};
    mem.dwLength = sizeof(mem);
    GlobalMemoryStatusEx(&mem);

    SYSTEM_INFO si{};
    GetNativeSystemInfo(&si);

    const std::string cpu = util::ToUtf8(wmi::QueryScalar(L"SELECT Name FROM Win32_Processor", L"Name"));
    const std::string gpu =
        util::ToUtf8(wmi::QueryScalar(L"SELECT Name FROM Win32_VideoController", L"Name"));
    const std::string osName =
        util::ToUtf8(wmi::QueryScalar(L"SELECT Caption FROM Win32_OperatingSystem", L"Caption"));

    const double totalGb = static_cast<double>(mem.ullTotalPhys) / (1024.0 * 1024.0 * 1024.0);
    const double freeGb = static_cast<double>(mem.ullAvailPhys) / (1024.0 * 1024.0 * 1024.0);

    if (HasFlag(a, "--json")) {
        json j;
        j["os"] = osName;
        j["build"] = os::GetWindowsBuild();
        j["cpu"] = cpu;
        j["cpuThreads"] = static_cast<int>(si.dwNumberOfProcessors);
        j["gpu"] = gpu;
        j["ramTotalGb"] = totalGb;
        j["ramFreeGb"] = freeGb;
        j["elevated"] = os::IsElevated();
        Out(j.dump(2));
        return 0;
    }

    char buf[256];
    Out("os       " + osName + " (build " + std::to_string(os::GetWindowsBuild()) + ")");
    Out("cpu      " + cpu + " (" + std::to_string(si.dwNumberOfProcessors) + " threads)");
    Out("gpu      " + gpu);
    snprintf(buf, sizeof(buf), "ram      %.1f GB total, %.1f GB free", totalGb, freeGb);
    Out(buf);
    Out(std::string("elevated ") + (os::IsElevated() ? "yes" : "no"));
    return 0;
}

int CmdProc(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: proc <list|kill> ...");
        return 2;
    }
    const std::string& sub = a.positional[0];

    if (sub == "list") {
        std::vector<pmgr::ProcRow> rows;
        pmgr::RefreshProcesses(rows, false);
        const std::wstring filter = util::ToWide(FlagValue(a, "--filter"));

        std::sort(rows.begin(), rows.end(),
                  [](const pmgr::ProcRow& x, const pmgr::ProcRow& y) { return x.mem > y.mem; });

        json arr = json::array();
        for (const auto& p : rows) {
            std::wstring hay = p.name + L" " + std::to_wstring(p.pid) + L" " + p.path;
            if (!filter.empty() && !pmgr::MatchesSearch(hay, filter)) continue;
            if (HasFlag(a, "--json")) {
                json j;
                j["pid"] = p.pid;
                j["name"] = util::ToUtf8(p.name);
                j["path"] = util::ToUtf8(p.path);
                j["memoryBytes"] = static_cast<uint64_t>(p.mem);
                j["critical"] = p.critical;
                arr.push_back(std::move(j));
                continue;
            }
            char line[512];
            snprintf(line, sizeof(line), "  %6lu  %9.1f MB  %s", p.pid,
                     static_cast<double>(p.mem) / (1024.0 * 1024.0),
                     util::ToUtf8(p.name).c_str());
            Out(line);
        }
        if (HasFlag(a, "--json")) Out(arr.dump(2));
        return 0;
    }

    if (sub == "kill") {
        if (a.positional.size() < 2) {
            Err("usage: proc kill <pid|name>");
            return 2;
        }
        const std::string& target = a.positional[1];
        const bool numeric = !target.empty() &&
                             target.find_first_not_of("0123456789") == std::string::npos;
        if (numeric) {
            const DWORD pid = static_cast<DWORD>(std::strtoul(target.c_str(), nullptr, 10));
            if (!pmgr::TerminateProcessByPid(pid)) {
                Err("error: could not terminate pid " + target);
                return 1;
            }
            Out("terminated pid " + target);
            return 0;
        }
        pmgr::TerminateProcessesByName(util::ToWide(target));
        Out("terminated " + target);
        return 0;
    }

    Err("error: unknown subcommand 'proc " + sub + "'");
    return 2;
}

int CmdSvc(const Args& a) {
    if (a.positional.empty() || a.positional[0] != "list") {
        Err("usage: svc list [--filter <text>] [--json]");
        return 2;
    }
    std::vector<pmgr::SvcRow> rows;
    pmgr::RefreshServices(rows);
    const std::wstring filter = util::ToWide(FlagValue(a, "--filter"));

    json arr = json::array();
    for (const auto& s : rows) {
        if (!filter.empty() && !pmgr::MatchesSearch(s.name + L" " + s.displayName, filter)) continue;
        if (HasFlag(a, "--json")) {
            json j;
            j["name"] = util::ToUtf8(s.name);
            j["displayName"] = util::ToUtf8(s.displayName);
            j["state"] = util::ToUtf8(pmgr::ServiceStateText(s.state));
            j["startType"] = util::ToUtf8(pmgr::ServiceStartTypeText(s.startType));
            arr.push_back(std::move(j));
            continue;
        }
        char line[512];
        snprintf(line, sizeof(line), "  %-28s %-10s %-12s %s", util::ToUtf8(s.name).c_str(),
                 util::ToUtf8(pmgr::ServiceStateText(s.state)).c_str(),
                 util::ToUtf8(pmgr::ServiceStartTypeText(s.startType)).c_str(),
                 util::ToUtf8(s.displayName).c_str());
        Out(line);
    }
    if (HasFlag(a, "--json")) Out(arr.dump(2));
    return 0;
}

int CmdUwp(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: uwp <list|remove> ...");
        return 2;
    }
    const std::string& sub = a.positional[0];

    if (sub == "list") {
        const auto result = uwp::ListInstalledPackages();
        if (result.packages.empty() && !result.error.empty()) {
            Err("error: " + result.error);
            return 1;
        }
        json arr = json::array();
        for (const auto& p : result.packages) {
            if (HasFlag(a, "--json")) {
                json j;
                j["name"] = util::ToUtf8(p.name);
                j["fullName"] = util::ToUtf8(p.packageFullName);
                arr.push_back(std::move(j));
                continue;
            }
            Out("  " + util::ToUtf8(p.packageFullName));
        }
        if (HasFlag(a, "--json")) Out(arr.dump(2));
        return 0;
    }

    if (sub == "remove") {
        std::vector<std::wstring> names;
        for (size_t i = 1; i < a.positional.size(); ++i) names.push_back(util::ToWide(a.positional[i]));
        if (names.empty()) {
            Err("usage: uwp remove <package-full-name> [...] --yes");
            return 2;
        }
        // Removing packages is irreversible without a reinstall, so require an
        // explicit acknowledgement rather than assuming intent from argv.
        if (!HasFlag(a, "--yes")) {
            Err("refusing to remove " + std::to_string(names.size()) +
                " package(s) without --yes");
            return 2;
        }
        if (!uwp::RemovePackages(names)) {
            Err("error: at least one package could not be removed");
            return 1;
        }
        Out("removed " + std::to_string(names.size()) + " package(s)");
        return 0;
    }

    Err("error: unknown subcommand 'uwp " + sub + "'");
    return 2;
}

int CmdShutdown(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: shutdown <minutes> | shutdown cancel");
        return 2;
    }
    if (a.positional[0] == "cancel") {
        proc::Run(L"shutdown", L"-a");
        Out("shutdown cancelled");
        return 0;
    }
    const int minutes = std::atoi(a.positional[0].c_str());
    if (minutes < 1) {
        Err("error: expected a positive number of minutes");
        return 2;
    }
    proc::Run(L"shutdown", L"-s -t " + std::to_wstring(minutes * 60));
    Out("shutdown scheduled in " + std::to_string(minutes) + " minute(s)");
    return 0;
}

// --- blocking ---------------------------------------------------------------

int CmdHosts(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: hosts <list|ban|unban|clear> ...");
        return 2;
    }
    const std::string& sub = a.positional[0];
    auto current = hosts::ReadBlockedDomains();

    if (sub == "list") {
        if (HasFlag(a, "--json")) {
            Out(json(current).dump(2));
        } else if (current.empty()) {
            Out("no domains blocked");
        } else {
            for (const auto& d : current) Out("  " + d);
        }
        return 0;
    }

    std::vector<std::string> next = current;
    if (sub == "ban") {
        for (size_t i = 1; i < a.positional.size(); ++i)
            for (const auto& d : hosts::ParseDomainsFromText(a.positional[i]))
                if (std::find(next.begin(), next.end(), d) == next.end()) next.push_back(d);
    } else if (sub == "unban") {
        for (size_t i = 1; i < a.positional.size(); ++i)
            for (const auto& d : hosts::ParseDomainsFromText(a.positional[i]))
                next.erase(std::remove(next.begin(), next.end(), d), next.end());
    } else if (sub == "clear") {
        next.clear();
    } else {
        Err("error: unknown subcommand 'hosts " + sub + "'");
        return 2;
    }

    if (hosts::ContainsForbiddenDomain(next)) {
        Err("error: that domain cannot be blocked");
        return 1;
    }
    if (RequireAdmin("editing the hosts file") != 0) return 1;

    std::wstring error;
    if (!hosts::UpdateBlockedDomains(next, &error)) {
        Err("error: " + util::ToUtf8(error));
        return 1;
    }
    Out(std::to_string(next.size()) + " domain(s) blocked");
    return 0;
}

int CmdApps(const Args& a) {
    if (a.positional.empty()) {
        Err("usage: apps <block|unblock> ...");
        return 2;
    }
    std::string list;
    if (a.positional[0] == "block") {
        for (size_t i = 1; i < a.positional.size(); ++i) {
            if (!list.empty()) list += ",";
            list += a.positional[i];
        }
        if (list.empty()) {
            Err("usage: apps block <exe> [...]");
            return 2;
        }
    } else if (a.positional[0] != "unblock") {
        Err("error: unknown subcommand 'apps " + a.positional[0] + "'");
        return 2;
    }

    if (disallow::ContainsForbiddenProcess(list)) {
        Err("error: that executable cannot be blocked");
        return 1;
    }
    std::wstring error;
    if (!disallow::ApplyProcessBlockList(list, &error)) {
        Err("error: " + util::ToUtf8(error));
        return 1;
    }
    Out(list.empty() ? "block list cleared" : "block list updated");
    return 0;
}

// --- settings ---------------------------------------------------------------

json SettingsToJson(const Settings& s) {
    json j;
    j["lang"] = s.lang;
    j["theme"] = s.theme;
    j["style"] = s.style;
    j["lastPageTag"] = s.lastPageTag;
    j["disableTelemetry"] = s.disableTelemetry;
    j["disableUpdateNotify"] = s.disableUpdateNotify;
    j["exclusiveMode"] = s.exclusiveMode;
    j["autoStartExclusive"] = s.autoStartExclusive;
    j["replaceTaskMgr"] = s.replaceTaskMgr;
    j["showSystemProcs"] = s.showSystemProcs;
    j["ramMbOnly"] = s.ramMbOnly;
    j["group"] = s.group;
    j["compact"] = s.compact;
    j["onlyFrozen"] = s.onlyFrozen;
    j["processExclusions"] = s.processExclusions;
    j["monitoringRefreshMs"] = s.monitoringRefreshMs;
    j["metricsServiceEnabled"] = s.metricsServiceEnabled;
    j["metricsIntervalSec"] = s.metricsIntervalSec;
    return j;
}

bool ApplySettingValue(Settings& s, const std::string& key, const std::string& value,
                       std::string& error) {
    bool flag = false;
    auto asBool = [&](bool& target) {
        if (!ParseBool(value, flag)) {
            error = "expected on/off for '" + key + "'";
            return false;
        }
        target = flag;
        return true;
    };
    auto asInt = [&](int& target, int lo, int hi) {
        const int v = std::atoi(value.c_str());
        if (v < lo || v > hi) {
            error = "expected " + std::to_string(lo) + ".." + std::to_string(hi) + " for '" + key + "'";
            return false;
        }
        target = v;
        return true;
    };

    if (key == "lang") { s.lang = value; return true; }
    if (key == "theme") {
        if (value != "Light" && value != "Dark") {
            error = "theme must be Light or Dark";
            return false;
        }
        s.theme = value;
        return true;
    }
    if (key == "style") { s.style = value; return true; }
    if (key == "lastPageTag") { s.lastPageTag = value; return true; }
    if (key == "processExclusions") { s.processExclusions = value; return true; }
    if (key == "disableTelemetry") return asBool(s.disableTelemetry);
    if (key == "disableUpdateNotify") return asBool(s.disableUpdateNotify);
    if (key == "exclusiveMode") return asBool(s.exclusiveMode);
    if (key == "autoStartExclusive") return asBool(s.autoStartExclusive);
    if (key == "replaceTaskMgr") return asBool(s.replaceTaskMgr);
    if (key == "showSystemProcs") return asBool(s.showSystemProcs);
    if (key == "ramMbOnly") return asBool(s.ramMbOnly);
    if (key == "group") return asBool(s.group);
    if (key == "compact") return asBool(s.compact);
    if (key == "onlyFrozen") return asBool(s.onlyFrozen);
    if (key == "metricsServiceEnabled") return asBool(s.metricsServiceEnabled);
    if (key == "monitoringRefreshMs") return asInt(s.monitoringRefreshMs, 200, 60000);
    if (key == "metricsIntervalSec") return asInt(s.metricsIntervalSec, 1, 3600);

    error = "unknown setting '" + key + "' (try: settings list)";
    return false;
}

int CmdSettings(const Args& a) {
    Settings s;
    s.Load();

    const std::string sub = a.positional.empty() ? "list" : a.positional[0];

    if (sub == "list") {
        const json j = SettingsToJson(s);
        if (HasFlag(a, "--json")) {
            Out(j.dump(2));
            return 0;
        }
        for (auto it = j.begin(); it != j.end(); ++it) {
            const std::string value =
                it->is_string() ? it->get<std::string>() : it->dump();
            Out("  " + it.key() + " = " + value);
        }
        return 0;
    }

    if (sub == "get") {
        if (a.positional.size() < 2) {
            Err("usage: settings get <key>");
            return 2;
        }
        const json j = SettingsToJson(s);
        if (!j.contains(a.positional[1])) {
            Err("error: unknown setting '" + a.positional[1] + "'");
            return 2;
        }
        const auto& v = j.at(a.positional[1]);
        Out(v.is_string() ? v.get<std::string>() : v.dump());
        return 0;
    }

    if (sub == "set") {
        if (a.positional.size() < 3) {
            Err("usage: settings set <key> <value>");
            return 2;
        }
        std::string error;
        if (!ApplySettingValue(s, a.positional[1], a.positional[2], error)) {
            Err("error: " + error);
            return 2;
        }
        s.Save();
        Out(a.positional[1] + " = " + a.positional[2]);
        return 0;
    }

    Err("error: unknown subcommand 'settings " + sub + "'");
    return 2;
}

int CmdAnalytics(const Args& a) {
    const std::string sub = a.positional.empty() ? "status" : a.positional[0];
    const auto consent = analytics::GetConsent();

    if (sub == "status") {
        const char* text = consent == AnalyticsConsent::Granted    ? "on"
                           : consent == AnalyticsConsent::Declined ? "off"
                                                                   : "unanswered";
        if (HasFlag(a, "--json")) {
            json j;
            j["consent"] = text;
            json tabs = json::array();
            for (const auto& [tag, count] : analytics::TopVisitedTabs(10)) {
                json entry;
                entry["tab"] = analytics::ScreenName(tag);
                entry["tag"] = tag;
                entry["visits"] = count;
                tabs.push_back(std::move(entry));
            }
            j["topTabs"] = std::move(tabs);
            Out(j.dump(2));
            return 0;
        }
        Out(std::string("analytics: ") + text);
        const auto tabs = analytics::TopVisitedTabs(10);
        if (!tabs.empty()) {
            Out("most visited tabs:");
            for (const auto& [tag, count] : tabs)
                Out("  " + std::to_string(count) + "  " + analytics::ScreenName(tag));
        }
        return 0;
    }

    if (sub == "on" || sub == "off") {
        analytics::SetConsent(sub == "on" ? AnalyticsConsent::Granted : AnalyticsConsent::Declined);
        Out(std::string("analytics: ") + sub);
        return 0;
    }

    if (sub == "reset") {
        // Makes the GUI ask again on next launch.
        analytics::SetConsent(AnalyticsConsent::Unknown);
        Out("analytics: unanswered (the app will ask again)");
        return 0;
    }

    Err("error: unknown subcommand 'analytics " + sub + "'");
    return 2;
}

int CmdVersion(const Args& a) {
    if (HasFlag(a, "--json")) {
        json j;
        j["name"] = brand::kDisplayNameUtf8;
        j["version"] = version::kText;
        j["windowsBuild"] = os::GetWindowsBuild();
        Out(j.dump(2));
        return 0;
    }
    Out(std::string(brand::kDisplayNameUtf8) + " " + version::kText);
    return 0;
}

} // namespace

bool WantsCli(int argc, wchar_t** argv) {
    if (argc < 2) return false;
    const std::wstring first = argv[1];
    if (first == L"--help" || first == L"-h" || first == L"/?" || first == L"--version")
        return true;
    // Legacy GUI switches all start with '-' or '/', so anything else is a
    // subcommand for us.
    return first[0] != L'-' && first[0] != L'/';
}

int Run(int argc, wchar_t** argv) {
    EnsureConsole();

    std::wstring cmdW = argc > 1 ? argv[1] : L"help";
    std::string cmd = util::ToUtf8(cmdW);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (cmd == "--help" || cmd == "-h" || cmd == "/?") cmd = "help";
    if (cmd == "--version") cmd = "version";

    const Args args = ParseArgs(argc, argv, 2);

    int code = 2;
    if (cmd == "help") {
        PrintUsage();
        code = 0;
    } else if (cmd == "tweak") {
        code = CmdTweak(args);
    } else if (cmd == "preset") {
        code = CmdPreset(args);
    } else if (cmd == "wu") {
        code = CmdWu(args);
    } else if (cmd == "cpu") {
        code = CmdCpu(args);
    } else if (cmd == "bench") {
        code = CmdBench(args);
    } else if (cmd == "sysinfo") {
        code = CmdSysInfo(args);
    } else if (cmd == "proc") {
        code = CmdProc(args);
    } else if (cmd == "svc") {
        code = CmdSvc(args);
    } else if (cmd == "uwp") {
        code = CmdUwp(args);
    } else if (cmd == "shutdown") {
        code = CmdShutdown(args);
    } else if (cmd == "hosts") {
        code = CmdHosts(args);
    } else if (cmd == "apps") {
        code = CmdApps(args);
    } else if (cmd == "analytics") {
        code = CmdAnalytics(args);
    } else if (cmd == "settings") {
        code = CmdSettings(args);
    } else if (cmd == "version") {
        code = CmdVersion(args);
    } else {
        Err("error: unknown command '" + cmd + "'");
        Err("run `MakuTweaker++.exe help` for the command list");
    }

    std::cout.flush();
    std::cerr.flush();
    ReleaseConsole();
    return code;
}

} // namespace maku::cli
