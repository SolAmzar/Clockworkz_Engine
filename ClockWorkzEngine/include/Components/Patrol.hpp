/******************************************************************************/
/*!
\file           Patrol.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Patrol component - pure data for patrol AI behavior.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"
#include <vector>

namespace Components {

/**
 * @brief Patrol component - patrol AI data
 *
 * Pure data component for entities that patrol between waypoints.
 * Used by AISystem to implement patrol behavior.
 */
struct Patrol {
  std::vector<vec2> waypoints{}; ///< Patrol waypoints
  int currentWaypoint{0};        ///< Current target waypoint index
  float waypointRadius{10.f};    ///< Distance to consider waypoint reached
  bool loop{true};               ///< Loop back to start after reaching end
  bool reverse{false}; ///< Reverse direction at end instead of looping
};

} // namespace Components
