/******************************************************************************/
/*!
\file           PlayerController.hpp
\project        Lights Out
\author(s)      [Lebon] (100%)
\brief          PlayerController — ECS tag component that marks the player
                entity.  All gameplay logic (movement, flashlight, animation)
                is handled by player_controller.lua via the LuaScript component.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

#include "ECS/Entity.hpp"
#include "ECS/World.hpp"
#include "Platform/InputManager.hpp"

namespace Components {

struct PlayerController {
    // Used by PlayerSystem to call OnEnter exactly once on first update.
    bool initialized{ false };

    void OnEnter(ECS::Entity, ECS::World&) {}
    void Update(ECS::Entity, ECS::World&, double, Platform::InputManager&) {}
    void OnExit(ECS::Entity, ECS::World&) {}
};

} // namespace Components
