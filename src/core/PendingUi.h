#pragma once

#include "core/Settings.h"

#include <functional>
#include <optional>
#include <string>

namespace maku::pending {

void Load();
void Save();

void StageBool(const char* id, bool value);
std::optional<bool> GetBool(const char* id);
void StageInt(const char* id, int value);
std::optional<int> GetInt(const char* id);
void Remove(const char* id);
bool HasAny();

void StageButton(const char* id);

using ToggleFn = std::function<void(bool)>;
void BindToggle(const char* id, bool* value, ToggleFn onChanged = nullptr);

using ButtonFn = std::function<void()>;
void BindButton(const char* id, ButtonFn onClick);

/// Restores automation/network settings from cache after elevated relaunch.
void ApplyMiscAfterElevation(Settings& settings);

} // namespace maku::pending
