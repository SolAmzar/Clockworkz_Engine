/******************************************************************************/
/*!
\file           Collider.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Collider component - pure data for collision detection.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"
#include <cstdint>

namespace Components {

/**
 * @brief Collider component - collision detection data
 *
 * Pure data component for AABB collision detection.
 * Used by CollisionSystem to detect and resolve collisions.
 */
struct Collider {
  vec2 size{100.f, 100.f}; ///< Collider dimensions (width, height)
  vec2 offset{0.f, 0.f};   ///< Offset from Transform position

  // Collision filtering
  uint32_t collisionMask{
      0xFFFFFFFF}; ///< What categories this collider can collide with
  uint32_t collisionCategory{
      0x00000001}; ///< What category this collider belongs to

  // Collision properties
  bool isSolid{true};      ///< Blocks movement
  bool isHazard{false};    ///< Deals damage on contact
  bool isTrigger{false};   ///< Doesn't block, just detects overlap
  bool blocksLight{false}; ///< Blocks light rays
  bool moveable{false};    ///< Can be pushed
};

} // namespace Components
