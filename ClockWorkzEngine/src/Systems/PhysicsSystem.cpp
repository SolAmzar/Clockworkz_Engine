/******************************************************************************/
/*!
\file           PhysicsSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          PhysicsSystem implementation - simple velocity integration.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/PhysicsSystem.hpp"
#include "Components/Layer.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"

namespace Systems {

void PhysicsSystem::update(ECS::World &world, double dt,
                           const std::vector<bool> *layerEnabled) {
  auto fdt = static_cast<float>(dt);

  // Query all entities with Transform + Velocity
  world.each<Components::Transform, Components::Velocity>(
      [&](ECS::Entity entity, Components::Transform &transform,
          Components::Velocity &velocity) {
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

        // Apply velocity to position
        // pos += vel * speed * dt
        transform.position.x += velocity.velocity.x * velocity.moveSpeed * fdt;
        transform.position.y += velocity.velocity.y * velocity.moveSpeed * fdt;
      });
}

} // namespace Systems
