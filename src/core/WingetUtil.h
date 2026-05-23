#pragma once
#include <string>
#include <vector>

namespace maku::winget {

struct AppEntry {
    std::string id;
    std::string label;
};

std::vector<AppEntry> Catalog();
bool Install(const std::string& id, std::string& logOut);
bool IsAvailable();

} // namespace maku::winget
