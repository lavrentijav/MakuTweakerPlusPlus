#include "ui/UiCommon.h"
#include "ui/Fonts.h"
#include "app/Application.h"
#include "core/LogFile.h"
#include "core/OsUtil.h"
#include "core/PendingUi.h"
#include "core/StringUtil.h"
#include <imgui.h>
#include <unordered_map>

namespace maku::ui {

void PageTitle(const std::string& text) {
    ImGui::PushFont(FontTitle());
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopFont();
    ImGui::Separator();
    ImGui::Spacing();
}

bool ToggleRow(const char* id, const std::string& label, bool* value,
               std::function<void(bool)> onChanged, const bool requiresAdmin,
               const bool rememberAcrossElevation) {
    ImGui::PushID(id);
    if (requiresAdmin || rememberAcrossElevation) pending::BindToggle(id, value, onChanged);

    bool prev = *value;
    ImGui::TextUnformatted(label.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
    bool changed = ImGui::Checkbox("##t", value);
    if (changed) {
        const bool needsElev = requiresAdmin && !os::HasAdminRights();
        if (needsElev || rememberAcrossElevation) {
            pending::StageBool(id, *value);
            pending::Save();
        }
        if (needsElev) {
            if (!os::EnsureAdmin(nullptr)) {
                pending::Remove(id);
                pending::Save();
                *value = prev;
            } else {
                *value = prev;
            }
        } else if (onChanged) {
            onChanged(*value);
            if (!rememberAcrossElevation) {
                pending::Remove(id);
                pending::Save();
            }
        }
    }
    ImGui::PopID();
    return changed && *value != prev;
}

bool ButtonRow(const char* id, const std::string& label, std::function<void()> onClick,
               const bool requiresAdmin, std::function<void()> rememberStateForElevation) {
    ImGui::PushID(id);
    if (requiresAdmin && onClick) pending::BindButton(id, onClick);

    bool clicked = ImGui::Button(label.c_str());
    if (clicked) {
        const bool needsElev = requiresAdmin && !os::HasAdminRights();
        if (needsElev) {
            if (rememberStateForElevation) rememberStateForElevation();
            pending::StageButton(id);
            pending::Save();
            if (!os::EnsureAdmin(nullptr)) {
                pending::Remove(id);
                pending::Save();
            }
        } else if (onClick) {
            onClick();
            pending::Remove(id);
            pending::Save();
        }
    }
    ImGui::PopID();
    return clicked;
}

void OnOffLabels() {}

namespace {

std::unordered_map<std::string, DWORD> g_pageSavedUntil;
std::unordered_map<std::string, bool> g_pageSavedOk;

} // namespace

void NotifyPageSaved(const char* pageKey, const bool ok) {
    if (!pageKey || !*pageKey) return;
    g_pageSavedOk[pageKey] = ok;
    g_pageSavedUntil[pageKey] = GetTickCount() + 3000;
}

void DrawSaveFooter(const char* pageKey, const std::function<bool()>& apply,
                    const bool requiresAdmin, std::function<void()> stageBeforeElevation,
                    const bool notifyOnApply) {
    if (!pageKey || !*pageKey) return;
    auto& l = app::Application::Instance().L10n();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    const std::string applyLabel = l.Def("apply");
    if (ImGui::Button((applyLabel + "##save_" + pageKey).c_str(), ImVec2(200, 0))) {
        const bool needsElev = requiresAdmin && !os::HasAdminRights();
        if (needsElev) {
            if (stageBeforeElevation) stageBeforeElevation();
            pending::Save();
            if (!os::EnsureAdmin(nullptr)) NotifyPageSaved(pageKey, false);
        } else {
            const bool ok = apply && apply();
            if (notifyOnApply) {
                logf::Info(std::string("Page save '") + pageKey + "': " + (ok ? "OK" : "FAILED") +
                            (os::HasAdminRights() ? " (admin)" : ""));
                NotifyPageSaved(pageKey, ok);
            } else if (!ok) {
                logf::Info(std::string("Page save '") + pageKey + ": FAILED (no selection?)");
                NotifyPageSaved(pageKey, false);
            }
        }
    }

    const auto untilIt = g_pageSavedUntil.find(pageKey);
    if (untilIt != g_pageSavedUntil.end() && GetTickCount() < untilIt->second) {
        const bool ok = g_pageSavedOk[pageKey];
        const ImVec4 color =
            ok ? ImVec4(0.35f, 0.95f, 0.55f, 1.f) : ImVec4(1.f, 0.45f, 0.4f, 1.f);
        ImGui::SameLine();
        const char* errKey =
            (!ok && requiresAdmin && os::HasAdminRights()) ? "save_apply_failed" : "save_failed";
        ImGui::TextColored(color, "%s", l.Def(ok ? "applied" : errKey).c_str());
    }
}

void TextWide(const std::wstring& text) {
    const std::string utf8 = util::ToUtf8(text);
    ImGui::TextUnformatted(utf8.c_str());
}

} // namespace maku::ui
