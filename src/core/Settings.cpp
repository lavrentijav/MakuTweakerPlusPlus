#include "core/Settings.h"
#include "core/StringUtil.h"
#include <fstream>
#include <windows.h>

namespace maku {

std::wstring Settings::Path() const {
    return util::GetAppDataPath() + L"\\settings.json";
}

void Settings::Load() {
    auto p = Path();
    std::ifstream f(p);
    if (!f) return;
    try {
        nlohmann::json j;
        f >> j;
        if (j.contains("lang")) lang = j["lang"].get<std::string>();
        if (j.contains("langSI")) langSI = j["langSI"].get<int>();
        if (j.contains("firRun")) firRun = j["firRun"].get<bool>();
        if (j.contains("theme")) theme = j["theme"].get<std::string>();
        if (j.contains("style")) style = j["style"].get<std::string>();
        if (j.contains("processExclusions"))
            processExclusions = j["processExclusions"].get<std::string>();
        if (j.contains("makuYanPar")) makuYanPar = j["makuYanPar"].get<std::string>();
        if (j.contains("lastPageTag")) lastPageTag = j["lastPageTag"].get<std::string>();
        if (j.contains("updIgnoredCount")) updIgnoredCount = j["updIgnoredCount"].get<int>();
        if (j.contains("disableUpdateNotify"))
            disableUpdateNotify = j["disableUpdateNotify"].get<bool>();
        if (j.contains("disableTelemetry")) disableTelemetry = j["disableTelemetry"].get<bool>();
        if (j.contains("analyticsConsent")) analyticsConsent = j["analyticsConsent"].get<int>();
        if (j.contains("tabVisits") && j["tabVisits"].is_object()) {
            for (auto it = j["tabVisits"].begin(); it != j["tabVisits"].end(); ++it)
                if (it->is_number_integer()) tabVisits[it.key()] = it->get<int>();
        }
        // Settings written before the consent prompt existed only carry
        // disableTelemetry. Treat an explicit `false` there as prior consent so
        // upgrading users are not re-prompted; anything else asks again.
        if (!j.contains("analyticsConsent") && j.contains("disableTelemetry"))
            analyticsConsent = static_cast<int>(disableTelemetry ? AnalyticsConsent::Declined
                                                                : AnalyticsConsent::Granted);
        disableTelemetry = analyticsConsent != static_cast<int>(AnalyticsConsent::Granted);
        if (j.contains("lastProcessFilterIndex"))
            lastProcessFilterIndex = j["lastProcessFilterIndex"].get<int>();
        if (j.contains("group")) group = j["group"].get<bool>();
        if (j.contains("compact")) compact = j["compact"].get<bool>();
        if (j.contains("onlyFrozen")) onlyFrozen = j["onlyFrozen"].get<bool>();
        if (j.contains("exclusiveMode")) exclusiveMode = j["exclusiveMode"].get<bool>();
        if (j.contains("autoStartExclusive"))
            autoStartExclusive = j["autoStartExclusive"].get<bool>();
        if (j.contains("replaceTaskMgr")) replaceTaskMgr = j["replaceTaskMgr"].get<bool>();
        if (j.contains("showSystemProcs")) showSystemProcs = j["showSystemProcs"].get<bool>();
        if (j.contains("ramMbOnly")) ramMbOnly = j["ramMbOnly"].get<bool>();
        if (j.contains("processViewMode")) processViewMode = j["processViewMode"].get<int>();
        if (j.contains("monitoringRefreshMs"))
            monitoringRefreshMs = j["monitoringRefreshMs"].get<int>();
        if (j.contains("cpuAutoTopK")) cpuAutoTopK = j["cpuAutoTopK"].get<int>();
        if (j.contains("pmgrShowMonitoring"))
            pmgrShowMonitoring = j["pmgrShowMonitoring"].get<bool>();
        if (j.contains("metricsServiceEnabled"))
            metricsServiceEnabled = j["metricsServiceEnabled"].get<bool>();
        if (j.contains("metricsIntervalSec"))
            metricsIntervalSec = j["metricsIntervalSec"].get<int>();
    } catch (...) {
    }
}

void Settings::Save() const {
    nlohmann::json j;
    j["lang"] = lang;
    j["langSI"] = langSI;
    j["firRun"] = firRun;
    j["theme"] = theme;
    j["style"] = style;
    j["processExclusions"] = processExclusions;
    j["makuYanPar"] = makuYanPar;
    j["lastPageTag"] = lastPageTag;
    j["updIgnoredCount"] = updIgnoredCount;
    j["disableUpdateNotify"] = disableUpdateNotify;
    // Derived, never authored by hand: consent is the source of truth.
    j["disableTelemetry"] = analyticsConsent != static_cast<int>(AnalyticsConsent::Granted);
    j["analyticsConsent"] = analyticsConsent;
    j["tabVisits"] = tabVisits;
    j["lastProcessFilterIndex"] = lastProcessFilterIndex;
    j["group"] = group;
    j["compact"] = compact;
    j["onlyFrozen"] = onlyFrozen;
    j["exclusiveMode"] = exclusiveMode;
    j["autoStartExclusive"] = autoStartExclusive;
    j["replaceTaskMgr"] = replaceTaskMgr;
    j["showSystemProcs"] = showSystemProcs;
    j["ramMbOnly"] = ramMbOnly;
    j["processViewMode"] = processViewMode;
    j["monitoringRefreshMs"] = monitoringRefreshMs;
    j["cpuAutoTopK"] = cpuAutoTopK;
    j["pmgrShowMonitoring"] = pmgrShowMonitoring;
    j["metricsServiceEnabled"] = metricsServiceEnabled;
    j["metricsIntervalSec"] = metricsIntervalSec;
    std::ofstream f(Path());
    if (f) f << j.dump(2);
}

std::string Settings::DetectSystemLanguage() {
    wchar_t lang[8]{};
    GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME, lang, 8);
    std::wstring lower = lang;
    for (auto& c : lower) c = towlower(c);
    std::string iso = util::ToUtf8(lower);
    if (iso == "fil") return "tl";
    const char* supported[] = {
        "uk", "cs", "ru", "az", "es", "tl", "tr", "ko", "zh", "it", "de", "fr", "be",
        "vi", "id", "hi", "ja", "kk", "pt", "lv", "fi", "et", "pl", "th"};
    for (auto s : supported)
        if (iso == s) return iso;
    return "en";
}

} // namespace maku
