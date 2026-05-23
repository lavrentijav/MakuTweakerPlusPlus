#pragma once
#include "core/Settings.h"
#include <string>

namespace maku::preset {

struct ApplyResult {
    bool ok = false;
    std::string message;
    bool needsReboot = false;
};

ApplyResult ApplyFile(const std::wstring& path, Settings& settings, bool safeMode);

} // namespace maku::preset
