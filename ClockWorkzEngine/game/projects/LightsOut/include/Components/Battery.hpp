/******************************************************************************/
/*!
\file           Battery.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Battery component - pure data for battery/energy system.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief Battery component - energy/power data
 *
 * Pure data component for entities with battery/energy mechanics.
 * Used by BatterySystem to handle charge/drain logic.
 * Typically used for the player character.
 */
struct Battery {
  float charge{100.f};    ///< Current battery charge
  float maxCharge{100.f}; ///< Maximum battery capacity
  float drainRate{5.f};   ///< Charge drain per second
  float chargeRate{20.f}; ///< Charge gain per second (when charging)
  bool isCharging{false}; ///< Currently charging flag
};

} // namespace Components
