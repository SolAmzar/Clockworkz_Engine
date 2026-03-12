/******************************************************************************/
/*!
\file           RenderSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon], 100%
\brief          RenderSystem implementation - renders sprites.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/RenderSystem.hpp"
#include "Assets/TextureManager.hpp"
#include <iostream>
#include <unordered_set>
#include "Components/Layer.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Graphics/RenderingManager.hpp"
#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <vector>

namespace Systems {

void RenderSystem::update(ECS::World &world,
                          Graphics::RenderingManager &renderer,
                          const std::vector<bool> *layerEnabled) {
  struct RenderEntry {
    Components::Transform *transform;
    Components::SpriteRenderer *sprite;
  };

  std::vector<RenderEntry> entries;

  // Collect all renderable entities
  world.each<Components::Transform, Components::SpriteRenderer>(
      [&](ECS::Entity entity, Components::Transform &transform,
          Components::SpriteRenderer &sprite) {
        if (!sprite.visible)
          return;

        // Check if entity's layer is enabled
        if (layerEnabled) {
          if (auto *layer = world.getComponent<Components::Layer>(entity)) {
            int idx = layer->index;
            if (idx >= 0 && idx < static_cast<int>(layerEnabled->size()) &&
                !(*layerEnabled)[idx]) {
              return; // Skip entities on disabled layers
            }
          }
        }

        entries.push_back({&transform, &sprite});
      });

  // Sort: lower sortOrder (layer) rendered first (behind), higher on top
  std::sort(entries.begin(), entries.end(),
            [](const RenderEntry &a, const RenderEntry &b) {
              return a.sprite->sortOrder < b.sprite->sortOrder;
            });

  // Track current render state
  glm::vec4 baseTint = renderer.getTint();
  bool isUnlitMode = false; // Assuming we start in 'Lit' mode (tinted)

  // Render in sorted order
  static std::unordered_set<std::string> s_warnedIds;
  for (auto &entry : entries) {
    auto texture = Assets::TextureManager::Get(entry.sprite->textureId);
    if (!texture && !entry.sprite->textureId.empty()) {
      if (s_warnedIds.insert(entry.sprite->textureId).second)
        std::cerr << "[RenderSystem] WARNING: textureId \"" << entry.sprite->textureId
                  << "\" not found — sprite will be invisible. Check Textures.json.\n";
    }
    unsigned texId = texture ? texture->id() : 0u;

    // Check if we need to switch shading mode
    if (entry.sprite->unlit != isUnlitMode) {
      renderer.flush(); // Finish current batch
      isUnlitMode = entry.sprite->unlit;
      if (isUnlitMode) {
        renderer.setTint(glm::vec4(1.0f)); // Full brightness
      } else {
        renderer.setTint(baseTint); // Apply scene tint
      }
    }

    glm::vec2 position(entry.transform->position.x,
                       entry.transform->position.y);
    glm::vec2 size(entry.sprite->size.x * entry.transform->scale.x,
                   entry.sprite->size.y * entry.transform->scale.y);

    renderer.drawSprite(
        texId, position, size, entry.transform->rotation,
        entry.sprite->uv, entry.sprite->color, false,
        glm::vec2(entry.transform->pivot.x, entry.transform->pivot.y));
  }

  // Restore state if we ended in Unlit mode
  if (isUnlitMode) {
    renderer.flush();
    renderer.setTint(baseTint);
  }
}

} // namespace Systems
