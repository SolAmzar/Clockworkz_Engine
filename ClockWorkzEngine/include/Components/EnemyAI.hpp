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
/**
 * @brief EnemyAI component
 *
 * Data component for state-machine based enemy behavior.
 */
struct EnemyAI {
  // State
  AIState state{AIState::Patrol};

  // State Data
  float stateTimer{0.f}; ///< General timer for states (Alert, Idle)
  vec2 targetPosition{0.f,
                      0.f}; ///< Last known interesting position (Player pos)

  // Patrol Data
  vec2 wanderDirection{1.f, 0.f};
  float wanderTimer{0.f};
  float wanderInterval{2.0f};

  // Detection / Chase Data
  float detectionRadius{300.f}; ///< Range to spot player
  float chaseSpeed{150.f};      ///< Speed when chasing
  float patrolSpeed{50.f};      ///< Speed when patrolling

  // Alert Data
  float alertDuration{
      3.0f}; ///< How long to stay Alert before returning to Patrol

  // Light Interaction
  float lightSensitivity{
      0.3f}; ///< Speed multiplier when in light (0.0 = stop, 1.0 = full speed)
};

} // namespace Components
