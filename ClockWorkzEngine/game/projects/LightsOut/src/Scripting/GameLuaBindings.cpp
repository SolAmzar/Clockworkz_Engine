/******************************************************************************/
/*!
\file           GameLuaBindings.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Extends EntityRef with game component accessors backed by Lua
                script properties (components/battery.lua, fragment.lua, etc.)
                instead of C++ component structs. Engine remains clean.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Scripting/GameLuaBindings.hpp"

#pragma warning(push, 0)
#include <sol/sol.hpp>
#pragma warning(pop)

#include "Scripting/LuaManager.hpp"
#include "Scripting/EntityRef.hpp"
#include "Components/LuaScript.hpp"
#include "Components/FootstepAudio.hpp"
#include "Core/Engine.hpp"
#include "Runtime/LevelMusicManager.hpp"

namespace Scripting {

// ---------------------------------------------------------------------------
// Property accessor helpers � read/write typed values from a script entry
// ---------------------------------------------------------------------------
static float getPropF(EntityRef& e, const char* path, const char* key, float def = 0.f) {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return def;
    auto it = entry->properties.find(key);
    if (it == entry->properties.end()) return def;
    if (auto* v = std::get_if<float>(&it->second)) return *v;
    return def;
}
static void setPropF(EntityRef& e, const char* path, const char* key, float val) {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return;
    auto it = entry->properties.find(key);
    if (it != entry->properties.end() && std::get_if<float>(&it->second)) it->second = val;
}
static bool getPropB(EntityRef& e, const char* path, const char* key, bool def = false) {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return def;
    auto it = entry->properties.find(key);
    if (it == entry->properties.end()) return def;
    if (auto* v = std::get_if<bool>(&it->second)) return *v;
    return def;
}
static void setPropB(EntityRef& e, const char* path, const char* key, bool val) {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return;
    auto it = entry->properties.find(key);
    if (it != entry->properties.end() && std::get_if<bool>(&it->second)) it->second = val;
}
static int getPropI(EntityRef& e, const char* path, const char* key, int def = 0) {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return def;
    auto it = entry->properties.find(key);
    if (it == entry->properties.end()) return def;
    if (auto* v = std::get_if<int>(&it->second)) return *v;
    return def;
}
static void setPropI(EntityRef& e, const char* path, const char* key, int val) {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return;
    auto it = entry->properties.find(key);
    if (it != entry->properties.end() && std::get_if<int>(&it->second)) it->second = val;
}
static std::string getPropS(EntityRef& e, const char* path, const char* key, const std::string& def = "") {
    auto* entry = e.findScriptEntry(path);
    if (!entry) return def;
    auto it = entry->properties.find(key);
    if (it == entry->properties.end()) return def;
    if (auto* v = std::get_if<std::string>(&it->second)) return *v;
    return def;
}

void RegisterGameBindings(LuaManager& luaMgr) {
    sol::state& lua = luaMgr.GetState();
    sol::usertype<EntityRef> er = lua["EntityRef"];

    // --- Battery (components/battery.lua) ---
    er["hasBattery"]          = [](EntityRef& e) { return e.hasScript("components/battery.lua"); };
    er["getBatteryCharge"]    = [](EntityRef& e) { return getPropF(e, "components/battery.lua", "charge"); };
    er["setBatteryCharge"]    = [](EntityRef& e, float v) { setPropF(e, "components/battery.lua", "charge", v); };
    er["getBatteryMaxCharge"] = [](EntityRef& e) { return getPropF(e, "components/battery.lua", "maxCharge", 100.f); };
    er["getBatteryDrainRate"] = [](EntityRef& e) { return getPropF(e, "components/battery.lua", "drainRate", 5.f); };
    er["getBatteryChargeRate"]= [](EntityRef& e) { return getPropF(e, "components/battery.lua", "chargeRate", 20.f); };
    er["isBatteryCharging"]   = [](EntityRef& e) { return getPropB(e, "components/battery.lua", "isCharging"); };
    er["setBatteryCharging"]  = [](EntityRef& e, bool v) { setPropB(e, "components/battery.lua", "isCharging", v); };

    // --- Fragment (components/fragment.lua) ---
    er["hasFragment"]            = [](EntityRef& e) { return e.hasScript("components/fragment.lua"); };
    er["isFragmentCollected"]    = [](EntityRef& e) { return getPropB(e, "components/fragment.lua", "collected"); };
    er["setFragmentCollected"]   = [](EntityRef& e, bool v) { setPropB(e, "components/fragment.lua", "collected", v); };
    er["getFragmentBobSpeed"]    = [](EntityRef& e) { return getPropF(e, "components/fragment.lua", "bobSpeed", 2.f); };
    er["getFragmentBobAmount"]   = [](EntityRef& e) { return getPropF(e, "components/fragment.lua", "bobAmount", 10.f); };
    er["getFragmentAnimTimer"]   = [](EntityRef& e) { return getPropF(e, "components/fragment.lua", "_timer"); };
    er["setFragmentAnimTimer"]   = [](EntityRef& e, float v) { setPropF(e, "components/fragment.lua", "_timer", v); };
    er["getFragmentAlpha"]       = [](EntityRef& e) { return getPropF(e, "components/fragment.lua", "_alpha", 1.f); };
    er["setFragmentAlpha"]       = [](EntityRef& e, float v) { setPropF(e, "components/fragment.lua", "_alpha", v); };
    er["getFragmentRequiresLight"]= [](EntityRef& e) { return getPropB(e, "components/fragment.lua", "requiresLight", true); };

    // --- FragmentCollector (components/fragment_collector.lua) ---
    er["hasFragmentCollector"]  = [](EntityRef& e) { return e.hasScript("components/fragment_collector.lua"); };
    er["getCollectedCount"]     = [](EntityRef& e) { return getPropI(e, "components/fragment_collector.lua", "collectedCount"); };
    er["setCollectedCount"]     = [](EntityRef& e, int v) { setPropI(e, "components/fragment_collector.lua", "collectedCount", v); };
    er["getRequiredFragments"]  = [](EntityRef& e) { return getPropI(e, "components/fragment_collector.lua", "requiredFragments"); };
    er["getPickupRadius"]       = [](EntityRef& e) { return getPropF(e, "components/fragment_collector.lua", "pickupRadius", 50.f); };

    // --- Door (components/door.lua) ---
    er["hasDoor"]               = [](EntityRef& e) { return e.hasScript("components/door.lua"); };
    er["isDoorOpen"]            = [](EntityRef& e) { return getPropB(e, "components/door.lua", "isOpen"); };
    er["setDoorOpen"]           = [](EntityRef& e, bool v) { setPropB(e, "components/door.lua", "isOpen", v); };
    er["getDoorRequiredFragments"]= [](EntityRef& e) { return getPropI(e, "components/door.lua", "requiredFragments", 1); };
    er["getDoorOpenTexture"]    = [](EntityRef& e) { return getPropS(e, "components/door.lua", "openTexture", "door_open"); };
    er["getDoorClosedTexture"]  = [](EntityRef& e) { return getPropS(e, "components/door.lua", "closedTexture", "door_close"); };

    // --- DoorSwing (components/door_swing.lua) ---
    er["hasDoorSwing"]              = [](EntityRef& e) { return e.hasScript("components/door_swing.lua"); };
    er["isDoorSwingOpen"]           = [](EntityRef& e) { return getPropB(e, "components/door_swing.lua", "isOpen"); };
    er["setDoorSwingOpen"]          = [](EntityRef& e, bool v) { setPropB(e, "components/door_swing.lua", "isOpen", v); };
    er["isDoorSwingInitialized"]    = [](EntityRef& e) { return getPropB(e, "components/door_swing.lua", "_initialized"); };
    er["setDoorSwingInitialized"]   = [](EntityRef& e, bool v) { setPropB(e, "components/door_swing.lua", "_initialized", v); };
    er["getDoorSwingInitialRotation"]= [](EntityRef& e) { return getPropF(e, "components/door_swing.lua", "initialRotation"); };
    er["setDoorSwingInitialRotation"]= [](EntityRef& e, float v) { setPropF(e, "components/door_swing.lua", "initialRotation", v); };
    er["getDoorSwingAngle"]         = [](EntityRef& e) { return getPropF(e, "components/door_swing.lua", "swingAngle", 90.f); };
    er["isDoorSwingClockwise"]      = [](EntityRef& e) { return getPropB(e, "components/door_swing.lua", "clockwise", true); };
    er["getDoorSwingSpeed"]         = [](EntityRef& e) { return getPropF(e, "components/door_swing.lua", "speed", 90.f); };

    // --- LightExposure (components/light_exposure.lua) ---
    er["hasLightExposure"]      = [](EntityRef& e) { return e.hasScript("components/light_exposure.lua"); };
    er["getLightExposureTime"]  = [](EntityRef& e) { return getPropF(e, "components/light_exposure.lua", "exposureTime"); };
    er["setLightExposureTime"]  = [](EntityRef& e, float v) { setPropF(e, "components/light_exposure.lua", "exposureTime", v); };
    er["getMaxLightExposureTime"]= [](EntityRef& e) { return getPropF(e, "components/light_exposure.lua", "maxExposureTime", 10.f); };
    er["isInLight"]             = [](EntityRef& e) { return getPropB(e, "components/light_exposure.lua", "inLight"); };
    er["setInLight"]            = [](EntityRef& e, bool v) { setPropB(e, "components/light_exposure.lua", "inLight", v); };
    er["isExposureDangerous"]   = [](EntityRef& e) { return getPropB(e, "components/light_exposure.lua", "isDangerous"); };

    // --- PlayerController � tag-based check (player_controller.lua) ---
    er["hasPlayerController"]   = [](EntityRef& e) { return e.hasScript("player_controller.lua"); };
    er["hasFootstepAudio"]      = [](EntityRef& e) {
        auto* world = e.getWorld();
        return world && world->hasComponent<Components::FootstepAudio>(e.getEntity());
    };

    sol::table audio = lua["Audio"];
    if (!audio.valid()) {
        audio = lua.create_table();
        lua["Audio"] = audio;
    }

    audio.set_function("requestMusicContext", [](const std::string& contextId) {
        return Runtime::LevelMusicManager::RequestContext(contextId, Core::Engine::Get().audio());
    });
    audio.set_function("stopMusic", [](sol::optional<float> fadeOutSeconds) {
        Runtime::LevelMusicManager::Stop(Core::Engine::Get().audio(),
                                         fadeOutSeconds.value_or(0.5f));
    });
    audio.set_function("getCurrentMusicContext", []() {
        return Runtime::LevelMusicManager::CurrentContext();
    });
    audio.set_function("getCurrentMusicTrack", []() {
        return Runtime::LevelMusicManager::CurrentTrack();
    });
    audio.set_function("hasMusicContext", [](const std::string& contextId) {
        return Runtime::LevelMusicManager::HasContext(contextId);
    });
}

} // namespace Scripting
