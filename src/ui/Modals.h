#pragma once

namespace maku::ui::modals {

void DrawAll();

void OpenSiteBan();
/// First-run analytics question. Modal and unskippable: the user has to pick.
void OpenAnalyticsConsent();
void OpenMakuYan();
void OpenExclusionSettings();

} // namespace maku::ui::modals
