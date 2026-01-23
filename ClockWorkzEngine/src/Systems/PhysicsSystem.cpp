/******************************************************************************/
/*!
\file           PhysicsSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          PhysicsSystem implementation - simple velocity integration.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/PhysicsSystem.hpp"
#include "Components/Transform.hpp"
#include "Components/Velocity.hpp"

namespace Systems {

void PhysicsSystem::update(ECS::World &world, double dt) {
  auto fdt = static_cast<float>(dt);

  // Query all entities with Transform + Velocity
  world.each<Components::Transform, Components::Velocity>(
      [&](ECS::Entity entity, Components::Transform &transform,
          Components::Velocity &velocity) {
        // Apply velocity to position
        // pos += vel * speed * dt
        transform.position.x += velocity.velocity.x * velocity.moveSpeed * fdt;
        transform.position.y += velocity.velocity.y * velocity.moveSpeed * fdt;
      });
}

} // namespace Systems
