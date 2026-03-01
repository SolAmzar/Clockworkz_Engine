/******************************************************************************/
/*!
\file           TextRenderSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          TextRenderSystem - renders entities with Transform + TextRenderer.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#pragma once
#include "ECS/World.hpp"
#include <glm/mat4x4.hpp>

namespace Graphics {
    class RenderingManager;
}

namespace Systems {

    class TextRenderSystem {

    public:
      // Render text for all entities with TextRenderer
      void update(ECS::World &world, Graphics::RenderingManager &renderer,
                  const glm::mat4 &viewProj);
    };
} // namespace Systems
