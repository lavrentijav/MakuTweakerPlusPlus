#pragma once

namespace maku::platform {

/// WinRT apartment + DispatcherQueue + Windows App SDK bootstrap (unpackaged).
class WinAppSdkRuntime {
public:
    static bool Initialize();
    static void Shutdown();
    static bool IsReady();

private:
    struct Impl;
};

} // namespace maku::platform
