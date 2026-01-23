#include "Systems/PlayerSystem.hpp"
#include "Components/PlayerController.hpp"
#include "Components/SpriteAnimator.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>

namespace Systems {

void PlayerSystem::update(ECS::World &world, double dt,
                          Platform::InputManager &input) {
  world.each<Components::PlayerController>(
      [&](ECS::Entity entity, Components::PlayerController &pc) {
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
