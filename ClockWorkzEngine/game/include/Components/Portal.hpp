/******************************************************************************/
/*!
\file           Portal.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          Component for level transition portals.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include <string>

namespace Components {

struct Portal {
  std::string
      levelName; // The filename of the level to load (e.g. "Level2.json")
};

} // namespace Components
