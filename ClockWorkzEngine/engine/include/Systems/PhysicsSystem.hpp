/******************************************************************************/
/*!
\file           PhysicsSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          PhysicsSystem - applies velocity to transform position.

                Second system in Phase 3. Queries entities with Transform and
                Velocity components and updates their position over time.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include <vector>

namespace Systems {

/**
 * @brief PhysicsSystem - handles movement physics
 *
 * Updates entity positions based on their velocity.
 * Formula: position += velocity * speed * dt
 */
class PhysicsSystem {
public:
  /**
   * @brief Update physics simulation
   * @param world ECS world containing entities
   * @param dt Delta time in seconds
   * @param layerEnabled Optional layer enabled flags (nullptr = all enabled)
   */
  void update(ECS::World &world, double dt,
              const std::vector<bool> *layerEnabled = nullptr);
};

} // namespace Systems
