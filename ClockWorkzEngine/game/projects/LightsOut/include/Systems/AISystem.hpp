/******************************************************************************/
/*!
\file           AISystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          AISystem - manages enemy behavior states.

                Handles Patrol, Chase, and Alert state transitions and
                movement logic.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include <vector>

namespace Systems {

/**
 * @brief AISystem - manages enemy AI states
 */
class AISystem {
public:
  /**
   * @brief Update enemy behaviors
   * @param world ECS world containing entities
   * @param dt Delta time for movement/timers
   * @param layerEnabled Optional layer enabled flags (nullptr = all enabled)
   */
  void update(ECS::World &world, double dt,
              const std::vector<bool> *layerEnabled = nullptr);
};

} // namespace Systems
