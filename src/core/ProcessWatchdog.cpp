#include "core/ProcessWatchdog.h"
#include "core/ProcessMgrUtil.h"
#include "core/Settings.h"
#include "core/StringUtil.h"
#include <atomic>
#include <thread>
#include <chrono>
#include <vector>

namespace maku::watchdog {
namespace {

std::atomic<bool> g_run{false};
std::atomic<bool> g_enabled{false};
std::thread g_worker;

std::vector<std::wstring> ParseBlocklist(const std::string& csv) {
    std::vector<std::wstring> out;
    std::wstring w = util::ToWide(csv);
    std::wstring token;
    for (wchar_t ch : w) {
        if (ch == L',' || ch == L';') {
            if (!token.empty()) {
                while (!token.empty() && iswspace(token.front())) token.erase(token.begin());
                while (!token.empty() && iswspace(token.back())) token.pop_back();
                if (token.find(L".exe") == std::wstring::npos) token += L".exe";
                out.push_back(token);
                token.clear();
            }
            continue;
        }
        token.push_back(ch);
    }
    if (!token.empty()) {
        if (token.find(L".exe") == std::wstring::npos) token += L".exe";
        out.push_back(token);
    }
    return out;
}

void WorkerLoop() {
    while (g_run.load()) {
        if (g_enabled.load()) {
            Settings s;
            s.Load();
            const auto blocked = ParseBlocklist(s.makuYanPar);
            if (!blocked.empty()) {
                std::vector<pmgr::ProcRow> procs;
                pmgr::RefreshProcesses(procs);
                for (const auto& p : procs) {
                    for (const auto& b : blocked) {
                        if (_wcsicmp(p.name.c_str(), b.c_str()) == 0) {
                            pmgr::KillProcess(p.pid);
                            break;
                        }
                    }
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

} // namespace

void Start() {
    if (g_run.exchange(true)) return;
    g_worker = std::thread(WorkerLoop);
}

void Stop() {
    if (!g_run.exchange(false)) return;
    if (g_worker.joinable()) g_worker.join();
}

void SetEnabled(const bool enabled) { g_enabled.store(enabled); }
bool IsEnabled() { return g_enabled.load(); }

} // namespace maku::watchdog
