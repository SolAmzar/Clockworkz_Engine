/******************************************************************************/
/*!
\file           CollisionSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          CollisionSystem implementation - AABB intersection.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/CollisionSystem.hpp"
#include "Components/Collider.hpp"
#include "Components/Transform.hpp"

namespace Systems {

void CollisionSystem::update(ECS::World &world) {
  m_collisions.clear();

  // Collect all entities with Transform + Collider
  // We need to collect them first because we cannot perform nested iteration
  // efficiently/safely with the current world.each() implementation
  std::vector<ECS::Entity> entities;
  world.each<Components::Transform, Components::Collider>(
      [&](ECS::Entity e, Components::Transform &, Components::Collider &) {
        entities.push_back(e);
      });

  // Simple O(N^2) broad phase for now
  // Optimization (spatial partitioning) comes in later phases
  for (size_t i = 0; i < entities.size(); ++i) {
    for (size_t j = i + 1; j < entities.size(); ++j) {
      auto eA = entities[i];
      auto eB = entities[j];

      // Retrieve components directly
      auto *tA = world.getComponent<Components::Transform>(eA);
      auto *cA = world.getComponent<Components::Collider>(eA);
      auto *tB = world.getComponent<Components::Transform>(eB);
      auto *cB = world.getComponent<Components::Collider>(eB);

      // Check for valid pointers (should be valid given the query above)
      if (tA && cA && tB && cB) {
        // Only check solid objects for collision logic
        // (Triggers might be separate later)
        if (cA->isSolid && cB->isSolid) {
          if (checkAABB(*tA, *cA, *tB, *cB)) {
            m_collisions.push_back({eA, eB});
          }
        }
      }
    }
  }
}

bool CollisionSystem::checkAABB(const Components::Transform &tA,
                                const Components::Collider &cA,
                                const Components::Transform &tB,
                                const Components::Collider &cB) {
  // Calculate bounds A
  float leftA = tA.position.x + cA.offset.x - cA.size.x * 0.5f;
  float rightA = leftA + cA.size.x;
  float topA = tA.position.y + cA.offset.y + cA.size.y * 0.5f;
  float bottomA = topA - cA.size.y;

  // Calculate bounds B
  float leftB = tB.position.x + cB.offset.x - cB.size.x * 0.5f;
  float rightB = leftB + cB.size.x;
  float topB = tB.position.y + cB.offset.y + cB.size.y * 0.5f;
  float bottomB = topB - cB.size.y;

  // AABB Intersection Test
  // Exit early if any gap is found
  if (leftA > rightB || rightA < leftB || bottomA > topB || topA < bottomB) {
    return false;
  }

  return true;
}

} // namespace Systems
