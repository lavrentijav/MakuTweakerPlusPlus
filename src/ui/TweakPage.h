#pragma once
#include <string>
#include <vector>

namespace maku::ui {

/// Renders every TweakRegistry entry whose `page` matches, as toggles followed
/// by action buttons. States are probed on a worker thread — some probes shell
/// out to bcdedit/powercfg and would otherwise stall the frame.
void DrawTweakPage(const char* page);

/// Forces the next frame to re-probe `page` (call after applying something
/// outside the page's own rows).
void InvalidateTweakPage(const char* page);

/// Checklist of tweak ids applied in one batch — the Quick Setup page.
void DrawQuickSetup();

/// Ids offered by Quick Setup, in display order. Shared with the CLI so
/// `preset apply quick` and the GUI button do the same thing.
const std::vector<std::string>& QuickSetupIds();

} // namespace maku::ui
