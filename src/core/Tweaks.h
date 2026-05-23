#pragma once
#include <functional>
#include <string>
#include <vector>

namespace maku::tweaks {

void SetWindowsUpdatesEnabled(bool enabled);
void ClearWindowsUpdateCache();
void ApplyTtlBypass();
void DebloatUwp();
using UwpProgressFn = std::function<void(int current, int total, const std::wstring& itemName)>;
/// Wildcards like *yourphone*; removes packages for all users + provisioned images.
bool RemoveUwpPackages(const std::vector<std::wstring>& patterns,
                       UwpProgressFn onProgress = nullptr);

} // namespace maku::tweaks
