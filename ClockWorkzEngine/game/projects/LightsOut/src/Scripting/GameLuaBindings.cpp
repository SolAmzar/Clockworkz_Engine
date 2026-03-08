/******************************************************************************/
/*!
\file           GameLuaBindings.cpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Extends the engine's EntityRef Lua usertype with LightsOut-
                specific component accessors.  The engine knows nothing about
                these components; they are wired in via the LuaExtensionFn
                callback registered in main.cpp.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Scripting/GameLuaBindings.hpp"

#pragma warning(push, 0)
#include <sol/sol.hpp>
#pragma warning(pop)

#include "Scripting/LuaManager.hpp"
#include "Scripting/EntityRef.hpp"
#include "ECS/World.hpp"

// LightsOut game components
#include "Components/Battery.hpp"
#include "Components/Fragment.hpp"
#include "Components/FragmentCollector.hpp"
#include "Components/Door.hpp"
#include "Components/DoorSwing.hpp"
#include "Components/LightExposure.hpp"
#include "Components/PlayerController.hpp"

namespace Scripting {

void RegisterGameBindings(LuaManager& luaMgr) {
    sol::state& lua = luaMgr.GetState();

    // Extend EntityRef with LightsOut-specific component accessors.
    // sol2 allows adding methods to an already-registered usertype table.
    sol::usertype<EntityRef> er = lua["EntityRef"];

    // --- Battery ---
    er["hasBattery"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::Battery>(e.getEntity());
    };
    er["getBatteryCharge"] = [](EntityRef& e) -> float {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        return b ? b->charge : 0.f;
    };
    er["setBatteryCharge"] = [](EntityRef& e, float c) {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        if (b) b->charge = c;
    };
    er["getBatteryMaxCharge"] = [](EntityRef& e) -> float {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        return b ? b->maxCharge : 0.f;
    };
    er["getBatteryDrainRate"] = [](EntityRef& e) -> float {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        return b ? b->drainRate : 0.f;
    };
    er["getBatteryChargeRate"] = [](EntityRef& e) -> float {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        return b ? b->chargeRate : 0.f;
    };
    er["isBatteryCharging"] = [](EntityRef& e) -> bool {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        return b ? b->isCharging : false;
    };
    er["setBatteryCharging"] = [](EntityRef& e, bool v) {
        auto* b = e.getWorld() ? e.getWorld()->getComponent<Components::Battery>(e.getEntity()) : nullptr;
        if (b) b->isCharging = v;
    };

    // --- Fragment ---
    er["hasFragment"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::Fragment>(e.getEntity());
    };
    er["isFragmentCollected"] = [](EntityRef& e) -> bool {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        return f ? f->collected : false;
    };
    er["setFragmentCollected"] = [](EntityRef& e, bool v) {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        if (f) f->collected = v;
    };
    er["getFragmentBobSpeed"] = [](EntityRef& e) -> float {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        return f ? f->bobSpeed : 0.f;
    };
    er["getFragmentBobAmount"] = [](EntityRef& e) -> float {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        return f ? f->bobAmount : 0.f;
    };
    er["getFragmentAnimTimer"] = [](EntityRef& e) -> float {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        return f ? f->animationTimer : 0.f;
    };
    er["setFragmentAnimTimer"] = [](EntityRef& e, float t) {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        if (f) f->animationTimer = t;
    };
    er["getFragmentAlpha"] = [](EntityRef& e) -> float {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        return f ? f->currentAlpha : 0.f;
    };
    er["setFragmentAlpha"] = [](EntityRef& e, float a) {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        if (f) f->currentAlpha = a;
    };
    er["getFragmentRequiresLight"] = [](EntityRef& e) -> bool {
        auto* f = e.getWorld() ? e.getWorld()->getComponent<Components::Fragment>(e.getEntity()) : nullptr;
        return f ? f->requiresLight : true;
    };

    // --- FragmentCollector ---
    er["hasFragmentCollector"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::FragmentCollector>(e.getEntity());
    };
    er["getCollectedCount"] = [](EntityRef& e) -> int {
        auto* c = e.getWorld() ? e.getWorld()->getComponent<Components::FragmentCollector>(e.getEntity()) : nullptr;
        return c ? c->collectedCount : 0;
    };
    er["setCollectedCount"] = [](EntityRef& e, int n) {
        auto* c = e.getWorld() ? e.getWorld()->getComponent<Components::FragmentCollector>(e.getEntity()) : nullptr;
        if (c) c->collectedCount = n;
    };
    er["getRequiredFragments"] = [](EntityRef& e) -> int {
        auto* c = e.getWorld() ? e.getWorld()->getComponent<Components::FragmentCollector>(e.getEntity()) : nullptr;
        return c ? c->requiredFragments : 0;
    };
    er["getPickupRadius"] = [](EntityRef& e) -> float {
        auto* c = e.getWorld() ? e.getWorld()->getComponent<Components::FragmentCollector>(e.getEntity()) : nullptr;
        return c ? c->pickupRadius : 50.f;
    };

    // --- Door ---
    er["hasDoor"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::Door>(e.getEntity());
    };
    er["isDoorOpen"] = [](EntityRef& e) -> bool {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::Door>(e.getEntity()) : nullptr;
        return d ? d->isOpen : false;
    };
    er["setDoorOpen"] = [](EntityRef& e, bool v) {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::Door>(e.getEntity()) : nullptr;
        if (d) d->isOpen = v;
    };
    er["getDoorRequiredFragments"] = [](EntityRef& e) -> int {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::Door>(e.getEntity()) : nullptr;
        return d ? d->requiredFragments : 1;
    };
    er["getDoorOpenTexture"] = [](EntityRef& e) -> std::string {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::Door>(e.getEntity()) : nullptr;
        return d ? d->openTexture : "";
    };
    er["getDoorClosedTexture"] = [](EntityRef& e) -> std::string {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::Door>(e.getEntity()) : nullptr;
        return d ? d->closedTexture : "";
    };

    // --- DoorSwing ---
    er["hasDoorSwing"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::DoorSwing>(e.getEntity());
    };
    er["isDoorSwingOpen"] = [](EntityRef& e) -> bool {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        return d ? d->isOpen : false;
    };
    er["setDoorSwingOpen"] = [](EntityRef& e, bool v) {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        if (d) d->isOpen = v;
    };
    er["isDoorSwingInitialized"] = [](EntityRef& e) -> bool {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        return d ? d->initialized : false;
    };
    er["setDoorSwingInitialized"] = [](EntityRef& e, bool v) {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        if (d) d->initialized = v;
    };
    er["getDoorSwingInitialRotation"] = [](EntityRef& e) -> float {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        return d ? d->initialRotation : 0.f;
    };
    er["setDoorSwingInitialRotation"] = [](EntityRef& e, float r) {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        if (d) d->initialRotation = r;
    };
    er["getDoorSwingAngle"] = [](EntityRef& e) -> float {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        return d ? d->swingAngle : 90.f;
    };
    er["isDoorSwingClockwise"] = [](EntityRef& e) -> bool {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        return d ? d->clockwise : true;
    };
    er["getDoorSwingSpeed"] = [](EntityRef& e) -> float {
        auto* d = e.getWorld() ? e.getWorld()->getComponent<Components::DoorSwing>(e.getEntity()) : nullptr;
        return d ? d->speed : 90.f;
    };

    // --- LightExposure ---
    er["hasLightExposure"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::LightExposure>(e.getEntity());
    };
    er["getLightExposureTime"] = [](EntityRef& e) -> float {
        auto* l = e.getWorld() ? e.getWorld()->getComponent<Components::LightExposure>(e.getEntity()) : nullptr;
        return l ? l->exposureTime : 0.f;
    };
    er["setLightExposureTime"] = [](EntityRef& e, float t) {
        auto* l = e.getWorld() ? e.getWorld()->getComponent<Components::LightExposure>(e.getEntity()) : nullptr;
        if (l) l->exposureTime = t;
    };
    er["getMaxLightExposureTime"] = [](EntityRef& e) -> float {
        auto* l = e.getWorld() ? e.getWorld()->getComponent<Components::LightExposure>(e.getEntity()) : nullptr;
        return l ? l->maxExposureTime : 10.f;
    };
    er["isInLight"] = [](EntityRef& e) -> bool {
        auto* l = e.getWorld() ? e.getWorld()->getComponent<Components::LightExposure>(e.getEntity()) : nullptr;
        return l ? l->inLight : false;
    };
    er["setInLight"] = [](EntityRef& e, bool v) {
        auto* l = e.getWorld() ? e.getWorld()->getComponent<Components::LightExposure>(e.getEntity()) : nullptr;
        if (l) l->inLight = v;
    };
    er["isExposureDangerous"] = [](EntityRef& e) -> bool {
        auto* l = e.getWorld() ? e.getWorld()->getComponent<Components::LightExposure>(e.getEntity()) : nullptr;
        return l ? l->isDangerous : false;
    };

    // --- PlayerController (tag check only) ---
    er["hasPlayerController"] = [](EntityRef& e) -> bool {
        auto* w = e.getWorld(); return w && w->hasComponent<Components::PlayerController>(e.getEntity());
    };
}

void RegisterGameWorldExtensions(ECS::World& world, sol::table& worldTable) {
    worldTable.set_function("eachFragment", [&world](sol::function fn) {
        world.each<Components::Fragment>([&](ECS::Entity e, Components::Fragment&) {
            fn(Scripting::EntityRef(e, &world));
        });
    });
    worldTable.set_function("eachDoor", [&world](sol::function fn) {
        world.each<Components::Door>([&](ECS::Entity e, Components::Door&) {
            fn(Scripting::EntityRef(e, &world));
        });
    });
    worldTable.set_function("eachDoorSwing", [&world](sol::function fn) {
        world.each<Components::DoorSwing>([&](ECS::Entity e, Components::DoorSwing&) {
            fn(Scripting::EntityRef(e, &world));
        });
    });
    worldTable.set_function("eachLightExposure", [&world](sol::function fn) {
        world.each<Components::LightExposure>([&](ECS::Entity e, Components::LightExposure&) {
            fn(Scripting::EntityRef(e, &world));
        });
    });
}

} // namespace Scripting
