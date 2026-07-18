#pragma once

namespace maku::app {
enum class PageId;
}

namespace maku::ui::pages {

void Draw(maku::app::PageId page);
void DrawWinInfo();
void DrawPci();

} // namespace maku::ui::pages
