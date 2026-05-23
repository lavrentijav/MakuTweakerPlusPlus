#include "core/PendingUi.h"

#include "core/Automation.h"
#include "core/LogFile.h"
#include "core/DnsBench.h"
#include "core/OsUtil.h"
#include "core/StringUtil.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <mutex>
#include <unordered_map>

namespace maku::pending {
namespace {

std::mutex g_mu;
nlohmann::json g_data;
bool g_loaded = false;

struct ToggleBinding {
    bool* value = nullptr;
    ToggleFn onChanged;
};

struct ButtonBinding {
    ButtonFn onClick;
};

std::unordered_map<std::string, ToggleBinding> g_toggles;
std::unordered_map<std::string, ButtonBinding> g_buttons;

std::wstring CachePath() { return util::GetAppDataPath() + L"\\pending_ui.json"; }

void EnsureLoaded() {
    if (g_loaded) return;
    Load();
}

nlohmann::json& Bools() {
    if (!g_data.contains("bools")) g_data["bools"] = nlohmann::json::object();
    return g_data["bools"];
}

nlohmann::json& Buttons() {
    if (!g_data.contains("buttons")) g_data["buttons"] = nlohmann::json::array();
    return g_data["buttons"];
}

nlohmann::json& Ints() {
    if (!g_data.contains("ints")) g_data["ints"] = nlohmann::json::object();
    return g_data["ints"];
}

bool HasButton(const std::string& id) {
    for (const auto& b : Buttons()) {
        if (b.is_string() && b.get<std::string>() == id) return true;
    }
    return false;
}

void RemoveButton(const std::string& id) {
    auto& arr = Buttons();
    for (size_t i = 0; i < arr.size(); ++i) {
        if (arr[i].is_string() && arr[i].get<std::string>() == id) {
            arr.erase(i);
            break;
        }
    }
}

bool TryApplyToggle(const std::string& id, ToggleBinding& bind) {
    if (!bind.value) return false;
    auto& bools = Bools();
    if (!bools.contains(id)) return false;

    const bool staged = bools[id].get<bool>();
    *bind.value = staged;

    if (!os::IsElevated()) return false;
    if (bind.onChanged) bind.onChanged(staged);
    logf::Info("Pending toggle applied: " + id + " = " + (staged ? "true" : "false"));
    bools.erase(id);
    return true;
}

bool TryApplyButton(const std::string& id, ButtonBinding& bind) {
    if (!bind.onClick || !HasButton(id)) return false;
    if (!os::IsElevated()) return false;
    logf::Info("Pending button applied: " + id);
    bind.onClick();
    RemoveButton(id);
    return true;
}

} // namespace

void Load() {
    std::lock_guard lock(g_mu);
    g_data = nlohmann::json::object();
    g_data["bools"] = nlohmann::json::object();
    g_data["buttons"] = nlohmann::json::array();
    std::ifstream f(CachePath());
    if (f) {
        try {
            nlohmann::json j;
            f >> j;
            if (j.is_object()) g_data = std::move(j);
        } catch (...) {
        }
    }
    if (!g_data.contains("bools")) g_data["bools"] = nlohmann::json::object();
    if (!g_data.contains("buttons")) g_data["buttons"] = nlohmann::json::array();
    if (!g_data.contains("ints")) g_data["ints"] = nlohmann::json::object();
    g_loaded = true;
}

void Save() {
    std::lock_guard lock(g_mu);
    std::ofstream f(CachePath());
    if (f) f << g_data.dump(2);
}

void StageBool(const char* id, const bool value) {
    if (!id || !*id) return;
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    Bools()[id] = value;
}

std::optional<bool> GetBool(const char* id) {
    if (!id || !*id) return std::nullopt;
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    const auto& bools = Bools();
    if (!bools.contains(id)) return std::nullopt;
    return bools[id].get<bool>();
}

void StageInt(const char* id, const int value) {
    if (!id || !*id) return;
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    Ints()[id] = value;
}

std::optional<int> GetInt(const char* id) {
    if (!id || !*id) return std::nullopt;
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    const auto& ints = Ints();
    if (!ints.contains(id)) return std::nullopt;
    return ints[id].get<int>();
}

void Remove(const char* id) {
    if (!id || !*id) return;
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    Bools().erase(id);
    Ints().erase(id);
    RemoveButton(id);
}

bool HasAny() {
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    return !Bools().empty() || !Buttons().empty() || !Ints().empty();
}

void StageButton(const char* id) {
    if (!id || !*id) return;
    EnsureLoaded();
    std::lock_guard lock(g_mu);
    if (HasButton(id)) return;
    Buttons().push_back(id);
}

void BindToggle(const char* id, bool* value, ToggleFn onChanged) {
    if (!id || !*id || !value) return;
    EnsureLoaded();
    const std::string key(id);
    bool dirty = false;
    {
        std::lock_guard lock(g_mu);
        auto& bind = g_toggles[key];
        bind.value = value;
        if (onChanged) bind.onChanged = std::move(onChanged);
        dirty = TryApplyToggle(key, bind);
    }
    if (dirty) Save();
    if (auto staged = GetBool(id)) *value = *staged;
}

void BindButton(const char* id, ButtonFn onClick) {
    if (!id || !*id || !onClick) return;
    EnsureLoaded();
    const std::string key(id);
    bool dirty = false;
    {
        std::lock_guard lock(g_mu);
        g_buttons[key] = ButtonBinding{std::move(onClick)};
        dirty = TryApplyButton(key, g_buttons[key]);
    }
    if (dirty) Save();
}

void ApplyMiscAfterElevation(Settings& s) {
    if (!os::IsElevated()) return;
    EnsureLoaded();

    auto takeApply = [&](const char* id, const auto& applyFn) {
        if (auto v = GetBool(id)) {
            applyFn(*v);
            Remove(id);
        }
    };

    takeApply("autoTtlOnBoot", [&](bool on) {
        s.autoTtlOnBoot = on;
        if (on)
            auto_task::InstallBootTtl();
        else
            auto_task::RemoveBootTtl();
    });
    takeApply("autoWeeklyCleanup", [&](bool on) {
        s.autoWeeklyCleanup = on;
        if (on)
            auto_task::InstallWeeklyCleanup();
        else
            auto_task::RemoveWeeklyCleanup();
    });
    takeApply("gamingTcp", [&](bool on) {
        s.gamingTcp = on;
        dns::ApplyGamingTcpTweaks(on);
    });

    Save();
    s.Save();
}

} // namespace maku::pending
