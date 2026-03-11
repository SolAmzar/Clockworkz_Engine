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
      [&](ECS::Entity e, Components::Transform &transform,
          Components::TextRenderer &text) {
        (void)e;
        if (!text.visible || text.text.empty())
          return;

        auto font =
            Assets::ResourceManager<Assets::FontManager>::Get(text.fontId);
        if (!font)
          return;

        float scale = text.fontSize * transform.scale.x;

        float boxW = text.boxSize.x * transform.scale.x;
        float boxH = text.boxSize.y * transform.scale.y;

        // Alignment enum drives the horizontal anchor:
        //   Left   → gizmo = left edge  → boxLeft = pos.x
        //   Center → gizmo = center     → boxLeft = pos.x - boxW * 0.5
        //   Right  → gizmo = right edge → boxLeft = pos.x - boxW
        // Vertical: gizmo = top of text block → boxTop = pos.y
        float boxLeft, boxTop;
        switch (text.alignment) {
          case Components::TextAlignment::Center:
            boxLeft = transform.position.x - boxW * 0.5f;
            break;
          case Components::TextAlignment::Right:
            boxLeft = transform.position.x - boxW;
            break;
          default:
            boxLeft = transform.position.x;
            break;
        }
        boxTop = transform.position.y;

        // Apply padding: shrink the inner content area
        float padX = text.padding.x * transform.scale.x;
        float padY = text.padding.y * transform.scale.y;
        font->RenderTextInBox(text.text,
                              boxLeft + padX, boxTop - padY,
                              boxW - padX * 2.0f, boxH - padY * 2.0f,
                              scale, text.color, *shader,
                              static_cast<int>(text.alignment),
                              static_cast<int>(text.verticalAlign),
                              text.wordWrap,
                              text.lineSpacing);
      });

  renderer.endFontPass();
}

} // namespace Systems
