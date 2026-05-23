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
        if (j.contains("theme")) {
            theme = j["theme"].get<std::string>();
            if (theme == "Standard" || theme == "Mica") theme = "Dark";
            if (theme != "Light" && theme != "Dark") theme = "Dark";
        }
        if (j.contains("style")) style = j["style"].get<std::string>();
        if (j.contains("processExclusions"))
            processExclusions = j["processExclusions"].get<std::string>();
        if (j.contains("makuYanPar")) makuYanPar = j["makuYanPar"].get<std::string>();
        if (j.contains("lastPageTag")) lastPageTag = j["lastPageTag"].get<std::string>();
        if (j.contains("updIgnoredCount")) updIgnoredCount = j["updIgnoredCount"].get<int>();
        if (j.contains("disableUpdateNotify"))
            disableUpdateNotify = j["disableUpdateNotify"].get<bool>();
        if (j.contains("lastProcessFilterIndex"))
            lastProcessFilterIndex = j["lastProcessFilterIndex"].get<int>();
        if (j.contains("group")) group = j["group"].get<bool>();
        if (j.contains("compact")) compact = j["compact"].get<bool>();
        if (j.contains("onlyFrozen")) onlyFrozen = j["onlyFrozen"].get<bool>();
        if (j.contains("showSystemProcesses"))
            showSystemProcesses = j["showSystemProcesses"].get<bool>();
        if (j.contains("autoTtlOnBoot")) autoTtlOnBoot = j["autoTtlOnBoot"].get<bool>();
        if (j.contains("autoWeeklyCleanup"))
            autoWeeklyCleanup = j["autoWeeklyCleanup"].get<bool>();
        if (j.contains("processWatchdog")) processWatchdog = j["processWatchdog"].get<bool>();
        if (j.contains("shellContextMenu"))
            shellContextMenu = j["shellContextMenu"].get<bool>();
        if (j.contains("gamingTcp")) gamingTcp = j["gamingTcp"].get<bool>();
        if (j.contains("preferredDns")) preferredDns = j["preferredDns"].get<std::string>();
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
    j["lastProcessFilterIndex"] = lastProcessFilterIndex;
    j["group"] = group;
    j["compact"] = compact;
    j["onlyFrozen"] = onlyFrozen;
    j["showSystemProcesses"] = showSystemProcesses;
    j["autoTtlOnBoot"] = autoTtlOnBoot;
    j["autoWeeklyCleanup"] = autoWeeklyCleanup;
    j["processWatchdog"] = processWatchdog;
    j["shellContextMenu"] = shellContextMenu;
    j["gamingTcp"] = gamingTcp;
    j["preferredDns"] = preferredDns;
    std::ofstream f(Path());
    if (f) f << j.dump(2);
}

std::string Settings::DetectSystemLanguage() {
    wchar_t locale[16]{};
    if (GetUserDefaultUILanguage()) {
        LCID lcid = GetUserDefaultUILanguage();
        wchar_t lang[8]{}, region[8]{};
        GetLocaleInfoW(lcid, LOCALE_SISO639LANGNAME, lang, 8);
        GetLocaleInfoW(lcid, LOCALE_SISO3166CTRYNAME, region, 8);
        std::wstring full = std::wstring(lang) + L"-" + region;
        std::wstring lower = full;
        for (auto& c : lower) c = towlower(c);

        if (lower == L"zh-tw" || lower == L"zh-hk" || lower == L"zh-mo") return "tw";
        if (lower == L"zh-cn" || lower == L"zh-sg") return "zh";

        std::string iso = util::ToUtf8(lang);
        if (iso == "fil") return "tl";
        const char* supported[] = {
            "uk", "cs", "ru", "az", "es", "tl", "tr", "ko", "zh", "it", "de", "fr", "be",
            "vi", "id", "hi", "ja", "kk", "pt", "lv", "fi", "et", "pl", "th"};
        for (auto s : supported)
            if (iso == s) return iso;
    }
    return "en";
}

} // namespace maku
