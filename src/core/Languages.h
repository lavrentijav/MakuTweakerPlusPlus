#pragma once
#include <string>

namespace maku::l10n {

struct LanguageInfo {
    const char* tag;
    const char* nativeName;
};

/// All UI locales shipped in loc/*.json (native endonym for the settings combo).
inline constexpr LanguageInfo kLanguages[] = {
    {"en", "English"},
    {"ru", "Русский"},
    {"uk", "Українська"},
    {"be", "Беларуская"},
    {"kk", "Қазақша"},
    {"cs", "Čeština"},
    {"de", "Deutsch"},
    {"fr", "Français"},
    {"es", "Español"},
    {"it", "Italiano"},
    {"pt", "Português"},
    {"fi", "Suomi"},
    {"et", "Eesti"},
    {"lv", "Latviešu"},
    {"pl", "Polski"},
    {"az", "Azərbaycanca"},
    {"tr", "Türkçe"},
    {"zh", "中文 (简体)"},
    {"tw", "中文 (繁體)"},
    {"ja", "日本語"},
    {"ko", "한국어"},
    {"vi", "Tiếng Việt"},
    {"th", "ไทย"},
    {"id", "Bahasa Indonesia"},
    {"tl", "Filipino"},
    {"hi", "हिन्दी"},
};

inline constexpr int kLanguageCount =
    static_cast<int>(sizeof(kLanguages) / sizeof(kLanguages[0]));

inline int LanguageIndex(const std::string& tag) {
    for (int i = 0; i < kLanguageCount; ++i)
        if (tag == kLanguages[i].tag) return i;
    return 0;
}

inline bool IsSupportedLanguage(const std::string& tag) {
    for (int i = 0; i < kLanguageCount; ++i)
        if (tag == kLanguages[i].tag) return true;
    return false;
}

} // namespace maku::l10n
