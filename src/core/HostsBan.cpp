#include "core/HostsBan.h"
#include "core/StringUtil.h"
#include <algorithm>
#include <fstream>
#include <sstream>

namespace maku::hosts {
namespace {

constexpr const char* kStartTag = "# --- MakuTweaker Site Ban Start ---";
constexpr const char* kEndTag = "# --- MakuTweaker Site Ban End ---";

std::string NormalizeDomain(std::string d) {
    while (!d.empty() && (d.front() == ' ' || d.front() == '\t')) d.erase(d.begin());
    while (!d.empty() && (d.back() == ' ' || d.back() == '\t' || d.back() == '\r')) d.pop_back();
    for (auto& c : d) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    const char* prefixes[] = {"https://", "http://"};
    for (auto p : prefixes) {
        if (d.rfind(p, 0) == 0) {
            d = d.substr(strlen(p));
            break;
        }
    }
    const auto slash = d.find('/');
    if (slash != std::string::npos) d = d.substr(0, slash);
    return d;
}

} // namespace

std::wstring HostsPath() {
    wchar_t sys[MAX_PATH]{};
    GetSystemDirectoryW(sys, MAX_PATH);
    return std::wstring(sys) + L"\\drivers\\etc\\hosts";
}

std::vector<std::string> ReadBlockedDomains() {
    std::vector<std::string> out;
    std::ifstream f(HostsPath().c_str());
    if (!f) return out;
    bool inside = false;
    std::string line;
    while (std::getline(f, line)) {
        if (line.find(kStartTag) != std::string::npos) {
            inside = true;
            continue;
        }
        if (line.find(kEndTag) != std::string::npos) break;
        if (!inside) continue;
        if (line.rfind("127.0.0.1 ", 0) == 0) {
            auto d = NormalizeDomain(line.substr(10));
            if (!d.empty()) out.push_back(d);
        }
    }
    return out;
}

bool ContainsForbiddenDomain(const std::vector<std::string>& domains) {
    static const char* forbidden[] = {"adderly.top", "youtube.com/@makuadarii", "boosty.to/adderly"};
    for (const auto& d : domains) {
        for (auto f : forbidden) {
            if (d.find(f) != std::string::npos) return true;
        }
    }
    return false;
}

bool UpdateBlockedDomains(const std::vector<std::string>& domains, std::wstring* errorOut) {
    try {
        std::vector<std::string> lines;
        {
            std::ifstream in(HostsPath().c_str());
            std::string line;
            while (std::getline(in, line)) lines.push_back(line);
        }
        int startIdx = -1, endIdx = -1;
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].find(kStartTag) != std::string::npos) startIdx = static_cast<int>(i);
            if (lines[i].find(kEndTag) != std::string::npos) endIdx = static_cast<int>(i);
        }
        if (startIdx >= 0 && endIdx >= startIdx)
            lines.erase(lines.begin() + startIdx, lines.begin() + endIdx + 1);

        if (!domains.empty()) {
            lines.push_back(kStartTag);
            for (const auto& d : domains) lines.push_back("127.0.0.1 " + d);
            lines.push_back(kEndTag);
        }

        std::ofstream out(HostsPath().c_str(), std::ios::trunc);
        for (size_t i = 0; i < lines.size(); ++i) {
            out << lines[i];
            if (i + 1 < lines.size()) out << '\n';
        }
        return true;
    } catch (const std::exception& ex) {
        if (errorOut) *errorOut = util::ToWide(ex.what());
        return false;
    }
}

std::vector<std::string> ParseDomainsFromText(const std::string& text) {
    std::vector<std::string> domains;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        std::stringstream ls(line);
        std::string part;
        while (std::getline(ls, part, ',')) {
            auto d = NormalizeDomain(part);
            if (!d.empty() &&
                std::find(domains.begin(), domains.end(), d) == domains.end())
                domains.push_back(d);
        }
    }
    return domains;
}

std::vector<std::string> YandexPreset() {
    return {
        "yandex.com", "passport.yandex.ru", "yandex.ru", "ya.ru", "yandex.net", "yastatic.net",
        "dzen.ru", "kinopoisk.ru", "auto.ru", "music.yandex.ru", "disk.yandex.ru", "maps.yandex.ru",
        "mail.yandex.ru", "market.yandex.ru", "translate.yandex.ru", "browser.yandex.ru"};
}

} // namespace maku::hosts
