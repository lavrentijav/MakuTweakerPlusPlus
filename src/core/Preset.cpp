#include "core/Preset.h"
#include "core/ProcessRunner.h"
#include "core/Registry.h"
#include "core/Rollback.h"
#include "core/Tweaks.h"
#include <chrono>
#include <fstream>
#include <nlohmann/json.hpp>

namespace maku::preset {

ApplyResult ApplyFile(const std::wstring& path, Settings& settings, const bool safeMode) {
    ApplyResult r;
    std::ifstream f(path);
    if (!f) {
        r.message = "Cannot open preset file";
        return r;
    }
    try {
        const auto now = std::chrono::system_clock::now().time_since_epoch().count();
        rollback::CreateSnapshot(L"preset_" + std::to_wstring(now));

        nlohmann::json j;
        f >> j;

        if (!safeMode) {
            if (j.contains("lang")) settings.lang = j["lang"].get<std::string>();
            if (j.contains("theme")) settings.theme = j["theme"].get<std::string>();
            if (j.contains("processExclusions"))
                settings.processExclusions = j["processExclusions"].get<std::string>();
            if (j.contains("makuYanPar")) settings.makuYanPar = j["makuYanPar"].get<std::string>();
        }

        if (j.contains("tweaks") && j["tweaks"].is_object()) {
            const auto& t = j["tweaks"];
            if (t.value("disableUpdates", false)) {
                tweaks::SetWindowsUpdatesEnabled(false);
                r.needsReboot = true;
            }
            if (t.value("enableUpdates", false)) tweaks::SetWindowsUpdatesEnabled(true);
            if (t.value("clearUpdateCache", false)) {
                tweaks::ClearWindowsUpdateCache();
                r.needsReboot = true;
            }
            if (t.value("ttlBypass", false)) tweaks::ApplyTtlBypass();
            if (t.value("debloatUwp", false)) tweaks::DebloatUwp();
            if (t.value("telemetryOff", false)) {
                reg::SetDword(HKEY_LOCAL_MACHINE,
                              L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection",
                              L"AllowTelemetry", 0);
            }
            if (t.value("hibernateOff", false))
                proc::RunHidden("powercfg /hibernate off");
        }

        if (j.contains("blockAdd") && !safeMode) {
            const std::string csv = j["blockAdd"].get<std::string>();
            if (!settings.makuYanPar.empty()) settings.makuYanPar += ", ";
            settings.makuYanPar += csv;
        }

        settings.Save();
        r.ok = true;
        r.message = "Preset applied";
    } catch (const std::exception& ex) {
        r.message = ex.what();
    }
    return r;
}

} // namespace maku::preset
