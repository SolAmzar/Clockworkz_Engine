/******************************************************************************/
/*!
\file           Health.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Health component - pure data for health/damage system.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief Health component - health and damage data
 *
 * Pure data component for entities that can take damage.
 * Used by DamageSystem and HealthSystem.
 */
struct Health {
  float hp{10.f};           ///< Current health points
  float maxHp{10.f};        ///< Maximum health points
  float regenRate{0.f};     ///< HP regeneration per second
  bool invulnerable{false}; ///< Cannot take damage
  bool isDead{false};       ///< Death flag
};

} // namespace Components
