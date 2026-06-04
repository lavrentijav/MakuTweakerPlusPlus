#pragma once
#include <string>

namespace maku::preset {

bool ExportSettings(const std::wstring& path, std::wstring* errorOut);
bool ImportSettings(const std::wstring& path, std::wstring* errorOut);

} // namespace maku::preset
