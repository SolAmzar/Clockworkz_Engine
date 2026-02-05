/******************************************************************************/
/*!
\file           Layer.hpp
\project        ClockWorkz Engine
\brief          Layer component - determines which layer an entity belongs to.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once

namespace Components {

/**
 * @brief Layer component - layer membership for entities
 *
 * Used by systems to determine if an entity should be processed
 * based on whether its layer is enabled or disabled.
 */
struct Layer {
  int index{0}; ///< Which layer this entity belongs to (0-based)
};

} // namespace Components
