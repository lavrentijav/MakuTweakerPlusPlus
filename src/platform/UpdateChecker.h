#pragma once
#include <functional>
#include <string>

namespace maku::platform {

struct UpdateInfo {
    bool available = false;
    int latestBuild = 0;
};

void CheckForUpdateAsync(int currentBuild, std::function<void(UpdateInfo)> onDone);

} // namespace maku::platform
