#pragma once

namespace maku::tweak {

struct ExplorerToggles {
    bool nonremovable{};
    bool hidden{};
    bool extensions{};
    bool pcHome{};
    bool gallery{};
    bool showPc{};
    bool shortcut{};
};

struct WindowsUpdateToggles {
    bool blockInternet{};
    bool excludeDrivers{};
    bool serviceStopped{};
    bool disableReserves{};
};

struct SysRecToggles {
    bool telemetryOff{};
    bool uacOff{};
    bool hibernateOff{};
    bool smartScreenOff{};
    bool bingOff{};
};

struct PersonalizationToggles {
    bool darkTheme{};
    bool transparencyOff{};
    bool verboseBoot{};
    bool endTask{};
    bool oldContextMenu{};
};

struct AdvancedToggles {
    bool vbsOff{};
    bool ttlReduced{};
    bool indexingOff{};
};

ExplorerToggles ProbeExplorer();
WindowsUpdateToggles ProbeWindowsUpdate();
SysRecToggles ProbeSysRec();
PersonalizationToggles ProbePersonalization();
AdvancedToggles ProbeAdvanced();

} // namespace maku::tweak
