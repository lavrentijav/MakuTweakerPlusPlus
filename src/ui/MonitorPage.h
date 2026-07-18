#pragma once

namespace maku::ui::pages {

void DrawMonitor();
/// Clears SQLite-backed chart caches when leaving the Monitor page.
void OnMonitorPageClosed();

} // namespace maku::ui::pages
