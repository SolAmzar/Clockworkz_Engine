/******************************************************************************/
/*!
\file           AudioSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          AudioSystem - manages audio playback for entities.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

namespace Systems {

/**
 * @brief AudioSystem - manages sound playback
 */
class AudioSystem {
public:
  /**
   * @brief Update audio sources
   * @param world ECS world
   */
  void update(ECS::World &world);
};

} // namespace Systems
