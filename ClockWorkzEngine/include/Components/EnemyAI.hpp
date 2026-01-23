/******************************************************************************/
/*!
\file           EnemyAI.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          EnemyAI component - data for state-based enemy behavior.

                Supports Patrol, Chase, and Alert states.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"
#include <vector>

namespace Components {

/**
 * @brief AI State Enum
 */
enum class AIState {
  Patrol, ///< Moving between waypoints
  Chase,  ///< Pursuing a target
  Alert,  ///< Lost target, searching/waiting
  Death   ///< No movement, dead
};

/**
 * @brief EnemyAI component
 *
 * Data component for state-machine based enemy behavior.
 */
struct EnemyAI {
  // State
  AIState state{AIState::Patrol};

  // Patrol Data
  std::vector<vec2> waypoints{};
  int currentWaypoint{0};
  float waypointRadius{10.f};
  bool loop{true};
  bool reverse{false};

  // Detection / Chase Data
  float detectionRadius{150.f}; ///< Range to spot player
  float chaseSpeed{100.f};      ///< Speed when chasing
  float patrolSpeed{50.f};      ///< Speed when patrolling

  // Alert Data
  float alertTimer{0.f}; ///< Current timer value
  float searchDuration{
      3.0f}; ///< How long to stay Alert before returning to Patrol
};

} // namespace Components
