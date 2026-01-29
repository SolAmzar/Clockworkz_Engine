/******************************************************************************/
/*!
\file           TextRenderSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          TextRenderSystem implementation - renders text for entities.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/TextRenderSystem.hpp"
#include "Assets/FontManager.hpp"
#include "Assets/ResourceManager.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Shader.hpp"

namespace Systems {

void TextRenderSystem::update(ECS::World &world,
                              Graphics::RenderingManager &renderer,
                              const glm::mat4 &viewProj) {
  auto shader = Assets::ResourceManager<Graphics::Shader>::Get("text");
  if (!shader)
    return;

  renderer.beginFontPass(viewProj);

  world.each<Components::Transform, Components::TextRenderer>(
      [&](ECS::Entity, Components::Transform &transform,
          Components::TextRenderer &text) {
        if (!text.visible || text.text.empty())
          return;

        auto font =
            Assets::ResourceManager<Assets::FontManager>::Get(text.fontId);
        if (!font)
          return;

        float x = transform.position.x;
        float y = transform.position.y;
        float scale = text.fontSize * transform.scale.x;
        float boxW = text.boxSize.x * transform.scale.x;
        float boxH = text.boxSize.y * transform.scale.y;

        font->RenderTextInBox(text.text, x, y, boxW, boxH, scale, text.color,
                              *shader, static_cast<int>(text.alignment));
      });

  renderer.endFontPass();
}

} // namespace Systems
