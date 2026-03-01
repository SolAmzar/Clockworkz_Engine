/******************************************************************************/
/*!
\file           SpriteRenderer.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          SpriteRenderer component - pure data for sprite rendering.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "Math/Vec2.hpp"
#include <glm/vec4.hpp>
#include <string>

namespace Components {

/**
 * @brief SpriteRenderer component - sprite rendering data
 *
 * Pure data component for rendering sprites.
 * Used by RenderSystem to draw textured quads.
 */
struct SpriteRenderer {
  std::string textureId{};              ///< Texture asset ID
  glm::vec4 uv{0.0f, 0.0f, 1.0f, 1.0f}; ///< UV rect (x,y,w,h)
  vec2 size{100.f, 100.f};              ///< Sprite size in world units
  bool visible{true};                   ///< Visibility flag
  glm::vec4 color{1.f, 1.f, 1.f, 1.f};  ///< Tint color (RGBA)
  bool unlit{false}; ///< If true, ignores global lighting/tint
  int sortOrder{0};  ///< Render order (higher = rendered first / behind)
};

} // namespace Components
