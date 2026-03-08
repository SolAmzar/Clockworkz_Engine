/******************************************************************************/
/*!
\file           GameLuaBindings.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Game-specific Lua bindings extension.
                Registers LightsOut component accessors on EntityRef without
                touching engine code.

All content (c) 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once

#pragma warning(push, 0)
#include <sol/sol.hpp>
#pragma warning(pop)
#include "ECS/World.hpp"

namespace Scripting { class LuaManager; }

namespace Scripting {

/// Called once right after the engine's RegisterAllBindings().
/// Extends the EntityRef Lua usertype with LightsOut-specific component
/// accessors (Battery, Fragment, FragmentCollector, Door, DoorSwing,
/// LightExposure, PlayerController).
void RegisterGameBindings(LuaManager& lua);

/// Called every frame by ScriptSystem via Scene::setWorldTableExtension().
/// Re-registers the game-specific World.eachXxx() queries (eachFragment,
/// eachDoor, eachDoorSwing, eachLightExposure) so they live in game code,
/// not in the engine's ScriptSystem.
void RegisterGameWorldExtensions(ECS::World& world, sol::table& worldTable);

} // namespace Scripting
