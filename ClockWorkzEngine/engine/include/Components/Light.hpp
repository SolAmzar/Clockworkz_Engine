/******************************************************************************/
/*!
\file           Light.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 100%
\brief          Declares the engine light component used by lighting systems.

All content © 2026 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "Math/Vec2.hpp"
#include <glm/vec3.hpp>

namespace Components {

struct Light {
  bool active{true};
  vec2 offset{35.0f, -20.0f}; // Default offset from entity center
  glm::vec3 color{1.0f, 0.9f, 0.6f};
  float radius{300.0f};
  float angle{60.0f};    // Field of view in degrees
  float intensity{1.0f}; // Opacity/Brightness (0.0 to 1.0)
};

} // namespace Components
