/******************************************************************************/
/*!
\file           SpriteRenderer.cpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Implements SpriteRendererComponent, responsible for rendering
                textured sprites attached to GameObjects. Integrates with
                RenderingManager for batched 2D drawing.
All content � 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#include "Gameplay/Components/SpriteRenderer.hpp"
#include "Gameplay/GameObject.hpp"
#include "Core/Engine.hpp"
#include "Graphics/RenderingManager.hpp"      // for RenderingManager
#include "Graphics/Texture2D.hpp"             // for Texture2D
#include "Assets/ResourceManager.hpp"         // for ResourceManager
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <iostream>

namespace Gameplay {

    void SpriteRendererComponent::onRender(GameObject& owner) {
        using Assets::ResourceManager;
        using Graphics::Texture2D;

        // Resolve texture alias (local override or GameObject's textureId)
        std::string texAlias = m_textureAlias.empty() ? owner.textureId() : m_textureAlias;
        if (texAlias.empty()) return;

        auto texture = ResourceManager<Texture2D>::Get(texAlias);
        if (!texture) {
            std::cerr << "[SpriteRenderer] Missing texture: " << texAlias << "\n";
            return;
        }

        // Bind texture
        //texture->bind(0);

        // Access central RenderingManager (from Engine)
        Graphics::RenderingManager& renderer = Core::Engine::Get().rendering();
		glm::vec2 size = glm::vec2(owner.size().x, owner.size().y);
		glm::vec2 pos = glm::vec2(owner.transform().position.x, owner.transform().position.y);
		glm::vec2 scale = glm::vec2(owner.transform().scale.x, owner.transform().scale.y);

        renderer.drawSprite(texture->id(),
            pos,
            size * scale, owner.transform().rotationRad,
            m_tint);
    }

} // namespace Gameplay
