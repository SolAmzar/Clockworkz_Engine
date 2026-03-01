/******************************************************************************/
/*!
\file           Velocity.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Velocity component - pure data for movement.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"

namespace Components {

/**
 * @brief Velocity component - movement data
 *
 * Pure data component for physics-based movement.
 * Used by PhysicsSystem to update Transform positions.
 */
struct Velocity {
  vec2 velocity{0.f, 0.f}; ///< Current velocity vector
  float moveSpeed{100.f};  ///< Base movement speed
};

} // namespace Components
