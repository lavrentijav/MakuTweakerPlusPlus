#pragma once
#include <string>
#include <vector>

namespace maku::hosts {

std::wstring HostsPath();
std::vector<std::string> ReadBlockedDomains();
bool UpdateBlockedDomains(const std::vector<std::string>& domains, std::wstring* errorOut);
bool ContainsForbiddenDomain(const std::vector<std::string>& domains);
std::vector<std::string> YandexPreset();
std::vector<std::string> ParseDomainsFromText(const std::string& text);

} // namespace maku::hosts
