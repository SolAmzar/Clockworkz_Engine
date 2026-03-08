/******************************************************************************/
/*!
\file           FragmentSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          FragmentSystem - handles visual effects for fragments.

                Applies bobbing animation to uncollected fragments.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

namespace Systems {

/**
 * @brief FragmentSystem - manages fragment visuals/logic
 */
class FragmentSystem {
public:
  /**
   * @brief Update fragment animations
   * @param world ECS world containing entities
   * @param dt Delta time for animation
   */
  void update(ECS::World &world, double dt);
};

} // namespace Systems
