/******************************************************************************/
/*!
\file           StateMachineSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Implementation of StateMachineSystem.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/StateMachineSystem.hpp"
#include "Components/PlayerController.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/StateMachine.hpp"
#include "Components/Transform.hpp"

namespace Systems {

void StateMachineSystem::update(ECS::World &world, double dt) {
  world.each<Components::StateMachine>(
      [&](ECS::Entity entity, Components::StateMachine &sm) {
        (void)entity;
        // Update timer
        sm.stateTimer += static_cast<float>(dt);
      });
}

} // namespace Systems
