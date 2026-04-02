/******************************************************************************/
/*!
\file           PlayerSystem.cpp
\project        Lights Out
\author(s)      [Lebon] 65% [Yusuf] 35%
\brief          Implements player-specific gameplay updates for Lights Out.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Systems/PlayerSystem.hpp"
#include "Components/Layer.hpp"
#include "Components/PlayerController.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

namespace Systems {

void PlayerSystem::update(ECS::World &world, double dt,
                          Platform::InputManager &input,
                          const std::vector<bool> *layerEnabled) {
  world.each<Components::PlayerController>(
      [&](ECS::Entity entity, Components::PlayerController &pc) {
        // Check if entity's layer is enabled
        if (layerEnabled) {
          if (auto *layer = world.getComponent<Components::Layer>(entity)) {
            int idx = layer->index;
            if (idx >= 0 && idx < static_cast<int>(layerEnabled->size()) &&
                !(*layerEnabled)[idx]) {
              return; // Skip entities on disabled layers
            }
          }
        }

        // Check for initialization (Simulating Start/OnEnter)
        if (!pc.initialized) {
          pc.OnEnter(entity, world);
          pc.initialized = true;
        }

        // Update loop (Simulating Update)
        pc.Update(entity, world, dt, input);
      });
}

} // namespace Systems
