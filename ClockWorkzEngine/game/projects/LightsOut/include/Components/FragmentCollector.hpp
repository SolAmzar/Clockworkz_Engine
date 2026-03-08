/******************************************************************************/
/*!
\file           FragmentCollector.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          FragmentCollector component - pure data for collecting
fragments.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief FragmentCollector component - data for entity that collects fragments
 *
 * Pure data component for player/collector entity.
 * Used by FragmentSystem to track collection progress.
 */
struct FragmentCollector {
  int requiredFragments{0}; ///< Total fragments needed
  int collectedCount{0};    ///< Currently collected
  float pickupRadius{50.f}; ///< Distance to pickup fragments
};

} // namespace Components
