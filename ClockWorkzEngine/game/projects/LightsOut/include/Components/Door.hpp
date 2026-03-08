/******************************************************************************/
/*!
\file           Door.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Door component - data for door mechanics.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>

namespace Components {

/**
 * @brief Door component - logic for locked/unlocked doors
 *
 * Used by DoorSystem to check win/unlock conditions.
 */
struct Door {
  int requiredFragments{1};                ///< Fragments needed to unlock
  bool isOpen{false};                      ///< Is currently open?
  std::string openTexture{"door_open"};    ///< Texture when open
  std::string closedTexture{"door_close"}; ///< Texture when closed
};

} // namespace Components
