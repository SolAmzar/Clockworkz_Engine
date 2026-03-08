/******************************************************************************/
/*!
\file           LightExposureSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          LightExposureSystem - manages light toxicity.

                Tracks exposure time for entities in light and determines
                if they are in a dangerous state.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

namespace Systems {

/**
 * @brief LightExposureSystem - manages light exposure mechanics
 */
class LightExposureSystem {
public:
  /**
   * @brief Update exposure levels
   * @param world ECS world containing entities
   * @param dt Delta time for exposure accumulation
   */
  void update(ECS::World &world, double dt);
};

} // namespace Systems
