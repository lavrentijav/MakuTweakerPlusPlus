#pragma once
#include <string>

namespace maku::disallow {

bool ApplyProcessBlockList(const std::string& commaSeparated, std::wstring* errorOut);
bool ContainsForbiddenProcess(const std::string& normalizedList);

} // namespace maku::disallow
