/******************************************************************************/
/*!
\file           AnimationSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          AnimationSystem - updates SpriteAnimator components.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"

namespace Systems {

/**
 * @brief AnimationSystem
 *
 * Updates time for active animations and modifies the
 * associated SpriteRenderer's UV coordinates.
 */
class AnimationSystem {
public:
  void update(ECS::World &world, double dt);
};

} // namespace Systems
