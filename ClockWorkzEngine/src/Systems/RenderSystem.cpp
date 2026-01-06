/******************************************************************************/
/*!
\file           RenderSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Lebon]
\brief          RenderSystem implementation - renders sprites.

All content © 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/

#include "Systems/RenderSystem.hpp"
#include "Assets/TextureManager.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Graphics/RenderingManager.hpp"
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Systems {

void RenderSystem::update(ECS::World &world,
                          Graphics::RenderingManager &renderer) {
  // Query all entities with Transform + SpriteRenderer
  world.each<Components::Transform, Components::SpriteRenderer>(
      [&](ECS::Entity entity, Components::Transform &transform,
          Components::SpriteRenderer &sprite) {
        // Skip if not visible
        if (!sprite.visible)
          return;

        // Get texture from TextureManager
        auto texture = Assets::TextureManager::Get(sprite.textureId);
        if (!texture)
          return; // Texture not loaded

        // Convert vec2 to glm::vec2 and apply scale
        glm::vec2 position(transform.position.x, transform.position.y);
        glm::vec2 size(sprite.size.x * transform.scale.x,
                       sprite.size.y * transform.scale.y);

        // Draw sprite using RenderingManager
        renderer.drawSprite(texture->id(), position, size, transform.rotation,
                            sprite.uv, // Use UV from component
                            sprite.color,
                            false // not UI
        );
      });
}

} // namespace Systems
