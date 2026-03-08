/******************************************************************************/
/*!
\file           BatterySystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          BatterySystem - manages battery charge levels.

                Handles draining and charging logic for entities with
                Battery components.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

namespace Systems {

/**
 * @brief BatterySystem - manages entity power/charge
 */
class BatterySystem {
public:
  /**
   * @brief Update battery levels
   * @param world ECS world containing entities
   * @param dt Delta time for rate calculations
   */
  void update(ECS::World &world, double dt);
};

} // namespace Systems
