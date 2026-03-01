/******************************************************************************/
/*!
\file           RenderSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          RenderSystem - renders entities with Transform + SpriteRenderer.

                First system in Phase 3. Queries entities with Transform and
                SpriteRenderer components and draws them using RenderingManager.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include <vector>

namespace Graphics {
class RenderingManager;
}

namespace Systems {

/**
 * @brief RenderSystem - draws sprites for entities
 *
 * Queries all entities with Transform + SpriteRenderer components
 * and renders them using the RenderingManager.
 */
class RenderSystem {
public:
  /**
   * @brief Update the render system
   * @param world ECS world containing entities
   * @param renderer Rendering manager for drawing
   * @param layerEnabled Optional layer enabled flags (nullptr = all enabled)
   */
  void update(ECS::World &world, Graphics::RenderingManager &renderer,
              const std::vector<bool> *layerEnabled = nullptr);
};

} // namespace Systems
