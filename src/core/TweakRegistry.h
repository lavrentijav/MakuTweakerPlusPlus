#pragma once
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace maku::l10n {
class Localization;
}

namespace maku::tweak {

/// Single source of truth for every system tweak the app exposes.
///
/// Both the ImGui pages and the CLI iterate this table, so a tweak added here
/// shows up in the GUI *and* as `MakuTweaker.exe tweak set <id> on` with no
/// extra wiring.
enum class Kind {
    Toggle, ///< Reversible on/off state with a probe.
    Action, ///< One-shot command (cleanup, repair, feature install).
};

/// What the user has to do before the tweak takes effect.
enum class Reboot {
    None = 0,
    Restart = 1,        ///< Full reboot.
    ExplorerRestart = 2,///< Restart explorer.exe.
    AfterScan = 3,      ///< Reboot once the running scan finishes.
};

struct Tweak {
    /// Stable, dotted, lowercase id. This is the CLI surface — never rename one
    /// without leaving an alias, scripts depend on them.
    const char* id;

    const char* page;        ///< PageTag() of the page that hosts it.
    const char* locCategory; ///< loc/{lang}.json top-level category.
    const char* locSection;  ///< Section inside that category (nearly always "main").
    const char* locKey;      ///< Key holding the display label.
    const char* summary;     ///< English fallback, also used for CLI listings.

    Kind kind = Kind::Toggle;
    Reboot reboot = Reboot::None;
    bool needsAdmin = false;

    /// Build gating, mirroring the original's Visibility switches.
    /// 0 means "unbounded" on that end. Range is inclusive.
    int minBuild = 0;
    int maxBuild = 0;

    /// Toggle only. Reports the tweak's current state.
    std::function<bool()> probe;
    /// Toggle only. `true` applies the tweak, `false` restores the Windows default.
    std::function<void(bool)> apply;
    /// Action only.
    std::function<void()> action;
};

/// Whole table, in display order.
const std::vector<Tweak>& All();

/// Null when no tweak carries that id.
const Tweak* Find(std::string_view id);

/// Tweaks whose `page` matches, filtered by IsAvailable().
std::vector<const Tweak*> ForPage(std::string_view page);

/// False when the running Windows build falls outside [minBuild, maxBuild].
bool IsAvailable(const Tweak& t);

/// Localized label, falling back to `summary` when the loc file lacks the key.
std::string DisplayName(const Tweak& t, const l10n::Localization& l);

/// Reads `probe` behind a try/catch so a broken key never takes down a frame.
bool Probe(const Tweak& t);

/// Runs `apply`/`action`. Returns false when the tweak is unavailable on this build.
bool Apply(const Tweak& t, bool on);

// --- Windows Update target release pinning (a choice, not a toggle) ---

/// Release ids the original offers, oldest first.
const std::vector<std::string>& TargetReleaseOptions();
/// Empty when no pin is configured.
std::string GetTargetRelease();
void SetTargetRelease(const std::string& release);
void ClearTargetRelease();

// --- CPU throttle (PROCTHROTTLEMAX on the active power scheme) ---

/// Active scheme GUID, or empty when powercfg output cannot be parsed.
std::wstring ActivePowerScheme();
/// Current maximum processor state as a percentage, or -1 when unknown.
int GetCpuThrottleMax();
/// Applies `percent` (1-100) to both AC and DC. Returns false on a powercfg error.
bool SetCpuThrottleMax(int percent, std::string* error = nullptr);

} // namespace maku::tweak
