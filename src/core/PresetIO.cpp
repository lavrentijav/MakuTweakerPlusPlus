#include "core/PresetIO.h"
#include "app/Application.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace maku::preset {

bool ExportSettings(const std::wstring& path, std::wstring* errorOut) {
    try {
        auto& s = maku::app::Application::Instance().GetSettings();
        nlohmann::json j;
        j["lang"] = s.lang;
        j["theme"] = s.theme;
        j["processExclusions"] = s.processExclusions;
        j["makuYanPar"] = s.makuYanPar;
        j["group"] = s.group;
        j["compact"] = s.compact;
        j["onlyFrozen"] = s.onlyFrozen;
        j["replaceTaskMgr"] = s.replaceTaskMgr;
        j["showSystemProcs"] = s.showSystemProcs;
        j["ramMbOnly"] = s.ramMbOnly;
        j["processViewMode"] = s.processViewMode;
        std::ofstream f(path);
        if (!f) {
            if (errorOut) *errorOut = L"Cannot write file";
            return false;
        }
        f << j.dump(2);
        return true;
    } catch (...) {
        if (errorOut) *errorOut = L"Export failed";
        return false;
    }
}

bool ImportSettings(const std::wstring& path, std::wstring* errorOut) {
    try {
        std::ifstream f(path);
        if (!f) {
            if (errorOut) *errorOut = L"Cannot read file";
            return false;
        }
        nlohmann::json j;
        f >> j;
        auto& s = maku::app::Application::Instance().GetSettings();
        if (j.contains("lang")) s.lang = j["lang"].get<std::string>();
        if (j.contains("theme")) s.theme = j["theme"].get<std::string>();
        if (j.contains("processExclusions"))
            s.processExclusions = j["processExclusions"].get<std::string>();
        if (j.contains("makuYanPar")) s.makuYanPar = j["makuYanPar"].get<std::string>();
        if (j.contains("group")) s.group = j["group"].get<bool>();
        if (j.contains("compact")) s.compact = j["compact"].get<bool>();
        if (j.contains("onlyFrozen")) s.onlyFrozen = j["onlyFrozen"].get<bool>();
        if (j.contains("replaceTaskMgr")) s.replaceTaskMgr = j["replaceTaskMgr"].get<bool>();
        if (j.contains("showSystemProcs")) s.showSystemProcs = j["showSystemProcs"].get<bool>();
        if (j.contains("ramMbOnly")) s.ramMbOnly = j["ramMbOnly"].get<bool>();
        if (j.contains("processViewMode")) s.processViewMode = j["processViewMode"].get<int>();
        s.Save();
        maku::app::Application::Instance().ReloadLanguage();
        return true;
    } catch (...) {
        if (errorOut) *errorOut = L"Import failed";
        return false;
    }
}

} // namespace maku::preset
