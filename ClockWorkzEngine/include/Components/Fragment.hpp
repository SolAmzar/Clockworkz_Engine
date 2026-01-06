/******************************************************************************/
/*!
\file           Fragment.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Fragment component - pure data for collectible fragments.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief Fragment component - collectible fragment data
 *
 * Pure data component for collectible fragment objects.
 * Used by FragmentSystem to handle collection logic.
 */
struct Fragment {
  int fragmentId{0};     ///< Unique fragment identifier
  bool collected{false}; ///< Has been collected
  float bobSpeed{2.f};   ///< Visual bobbing speed
  float bobAmount{10.f}; ///< Visual bobbing amplitude
};

} // namespace Components
