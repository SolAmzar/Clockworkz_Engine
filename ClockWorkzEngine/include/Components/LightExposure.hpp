/******************************************************************************/
/*!
\file           LightExposure.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          LightExposure component - pure data for light exposure
mechanics.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief LightExposure component - light exposure tracking
 *
 * Pure data component for horror game light exposure mechanics.
 * Used by LightExposureSystem to track time spent in light/darkness.
 * Typically used for the player character.
 */
struct LightExposure {
  float exposureTime{0.f};     ///< Time spent in light (seconds)
  float maxExposureTime{10.f}; ///< Maximum safe exposure time
  bool inLight{false};         ///< Currently in light
  bool isDangerous{false};     ///< Exposure has reached dangerous levels
};

} // namespace Components
