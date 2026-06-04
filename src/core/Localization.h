#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace maku::l10n {

using StringMap = std::unordered_map<std::string, std::string>;

class Localization {
public:
    explicit Localization(const std::string& lang);

    const nlohmann::json& Root() const { return root_; }
    StringMap LoadCategory(const std::string& category) const;
    std::string Get(const std::string& category, const std::string& section,
                    const std::string& key) const;
    std::string CatName(const std::string& internalTag) const;
    std::string Def(const std::string& key) const;

    struct TweakSuggestion {
        std::string id;
        std::string displayName;
        std::string categoryKey;
        std::string internalCategoryTag;
    };
    std::vector<TweakSuggestion> GetAllTweaksForSearch() const;

    static std::wstring LocPath(const std::string& lang);

private:
    std::string lang_;
    nlohmann::json root_;
    void LoadFile(const std::string& lang);
};

} // namespace maku::l10n
