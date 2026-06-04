#pragma once
#include <string>

namespace maku::taskmgr {

bool IsReplaced();
bool SetReplace(bool enable, std::wstring* errorOut);

} // namespace maku::taskmgr
