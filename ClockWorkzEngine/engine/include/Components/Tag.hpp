
/******************************************************************************/
/*!
\file           Tag.hpp
\project        Lights Out
\author(s)      Lebon 100%
\brief          Declares the Tag component for entity naming/identification.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include <string>

namespace Components {

struct Tag {
  std::string tag;

  Tag(const std::string &name = "Entity") : tag(name) {}
};

} // namespace Components
