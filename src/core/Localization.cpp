#include "core/Localization.h"
#include "core/StringUtil.h"
#include <fstream>
#include <set>

namespace maku::l10n {

namespace {

const nlohmann::json* TryGet(const nlohmann::json& j, const char* key) {
    if (!j.is_object() || !j.contains(key)) return nullptr;
    return &j.at(key);
}

std::string GetString(const nlohmann::json& j, const std::string& fallback = {}) {
    if (j.is_string()) return j.get<std::string>();
    return fallback;
}

} // namespace

std::wstring Localization::LocPath(const std::string& lang) {
    return util::GetExeDirectory() + L"\\loc\\" + util::ToWide(lang) + L".json";
}

void Localization::LoadFile(const std::string& lang) {
    std::ifstream f(LocPath(lang));
    if (!f && lang != "en") {
        LoadFile("en");
        lang_ = "en";
        return;
    }
    if (!f) {
        root_ = nlohmann::json::object();
        return;
    }
    f >> root_;
    lang_ = lang;
}

Localization::Localization(const std::string& lang) : lang_(lang) { LoadFile(lang); }

StringMap Localization::LoadCategory(const std::string& category) const {
    StringMap result;
    const nlohmann::json* categories = TryGet(root_, "categories");
    if (!categories) return result;
    const nlohmann::json* cat = TryGet(*categories, category.c_str());
    if (!cat || !cat->is_object()) return result;

    for (auto it = cat->begin(); it != cat->end(); ++it) {
        if (!it.value().is_object()) continue;
        for (auto jt = it.value().begin(); jt != it.value().end(); ++jt) {
            if (jt.value().is_string())
                result[jt.key()] = jt.value().get<std::string>();
        }
    }
    return result;
}

std::string Localization::Get(const std::string& category, const std::string& section,
                              const std::string& key) const {
    const nlohmann::json* categories = TryGet(root_, "categories");
    if (!categories) return key;
    const nlohmann::json* cat = TryGet(*categories, category.c_str());
    if (!cat) return key;
    const nlohmann::json* sec = TryGet(*cat, section.c_str());
    if (!sec) return key;
    const nlohmann::json* val = TryGet(*sec, key.c_str());
    if (!val) return key;
    return GetString(*val, key);
}

std::string Localization::CatName(const std::string& internalTag) const {
    const nlohmann::json* categories = TryGet(root_, "categories");
    if (!categories) return internalTag;
    const nlohmann::json* base = TryGet(*categories, "base");
    if (!base) return internalTag;
    const nlohmann::json* catname = TryGet(*base, "catname");
    if (!catname) return internalTag;
    const nlohmann::json* val = TryGet(*catname, internalTag.c_str());
    if (!val) return internalTag;
    return GetString(*val, internalTag);
}

std::string Localization::Def(const std::string& key) const { return Get("base", "def", key); }

std::vector<Localization::TweakSuggestion> Localization::GetAllTweaksForSearch() const {
    std::vector<TweakSuggestion> tweaks;
    static const std::set<std::string> ignored = {
        "label", "choose", "showall", "info1", "info2", "info3", "chk", "comp", "b",
        "mode1", "mode2", "mode3", "tpmy", "tpmn", "tooltip", "test1multi",
        "running_multicore", "running", "test1", "test2", "test3", "benchtip",
        "info", "flyout", "reportbutton", "b2", "b4", "deledge_done", "deledge_error",
        "deledge_tooltip", "deledge_sure", "deledge_before", "deledge_btn",
        "makuos_tooltip", "infodone", "title", "title2", "os", "oned", "tenM",
        "thirtyM", "oneH", "twoH", "fourH", "sixH", "suredialogT1", "suredialogT2",
        "suredialogT3", "suredialogT4", "suredialogNS", "isnt", "is", "wu5b", "wu6b",
        "install", "reset", "enable", "e8b"};
    static const std::set<std::string> skipCats = {
        "base", "pmgr", "perfor", "settings", "ab", "quick"};

    const nlohmann::json* categories = TryGet(root_, "categories");
    if (!categories || !categories->is_object()) return tweaks;

    const nlohmann::json* base = TryGet(*categories, "base");
    const nlohmann::json* catNames = base ? TryGet(*base, "catname") : nullptr;

    for (auto& category : categories->items()) {
        if (skipCats.count(category.key())) continue;
        if (!category.value().is_object()) continue;
        const nlohmann::json* main = TryGet(category.value(), "main");
        if (!main || !main->contains("label")) continue;

        std::string tag = category.key();
        std::string displayCat = tag;
        if (catNames) {
            const nlohmann::json* name = TryGet(*catNames, tag.c_str());
            if (name) displayCat = GetString(*name, tag);
        }

        for (auto& tweak : main->items()) {
            std::string tkey = tweak.key();
            if (ignored.count(tkey)) continue;
            if (tkey.rfind("tooltip", 0) == 0 || tkey.rfind("desc", 0) == 0 ||
                tkey.rfind("status", 0) == 0 || tkey.rfind("info", 0) == 0 ||
                tkey.rfind("tip", 0) == 0 || tkey.rfind("note", 0) == 0 ||
                tkey.rfind("warn", 0) == 0 || tkey.rfind("msg", 0) == 0)
                continue;
            if (!tweak.value().is_string()) continue;
            std::string val = tweak.value().get<std::string>();
            if (val.size() > 90) continue;
            tweaks.push_back({tkey, val, displayCat, tag});
        }
    }
    return tweaks;
}

} // namespace maku::l10n
