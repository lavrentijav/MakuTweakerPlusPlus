#include "core/LogFile.h"

#include "core/StringUtil.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace maku::logf {
namespace {

std::mutex g_mutex;
std::ofstream g_out;
std::wstring g_path;
bool g_active = false;

void WriteLocked(const std::string& level, const std::string& line) {
    if (!g_active) return;
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm local{};
    localtime_s(&local, &t);
    g_out << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << " [" << level << "] " << line << '\n';
    g_out.flush();
}

} // namespace

void Init(const std::wstring& path) {
    std::lock_guard lock(g_mutex);
    if (g_active) g_out.close();
    g_path = path;
    std::error_code ec;
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent, ec);
    g_out.open(path, std::ios::app);
    g_active = g_out.is_open();
}

void InitDefault() {
    if (Active()) return;
    Init(util::GetAppDataPath() + L"\\logs\\MakuTweaker.log");
    Info("MakuTweaker++ session started");
}

std::wstring Path() {
    std::lock_guard lock(g_mutex);
    return g_path;
}

bool Active() {
    std::lock_guard lock(g_mutex);
    return g_active;
}

void Write(const std::string& line) { Info(line); }

void Info(const std::string& line) {
    std::lock_guard lock(g_mutex);
    WriteLocked("INFO", line);
}

void Warn(const std::string& line) {
    std::lock_guard lock(g_mutex);
    WriteLocked("WARN", line);
}

void Error(const std::string& line) {
    std::lock_guard lock(g_mutex);
    WriteLocked("ERROR", line);
}

void InfoWide(const std::wstring& line) { Info(util::ToUtf8(line)); }

} // namespace maku::logf
