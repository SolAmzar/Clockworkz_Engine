/******************************************************************************/
/*!
\file           Transform.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Transform component - pure data for position, rotation, and
scale.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"

namespace Components {

/**
 * @brief Transform component - position, rotation, and scale
 *
 * Pure data component for 2D spatial transformation.
 * Used by rendering, physics, and collision systems.
 */
struct Transform {
  vec2  position{0.f, 0.f}; ///< World position
  float rotation{0.f};      ///< Rotation in radians
  vec2  scale{1.f, 1.f};    ///< Scale factors (x, y)
  vec2  pivot{0.f, 0.f};    ///< Pivot offset from center (in local space units)
  bool  snapToGrid{false};  ///< If true, this entity snaps to grid when dragged (overrides global off)
};

} // namespace Components
