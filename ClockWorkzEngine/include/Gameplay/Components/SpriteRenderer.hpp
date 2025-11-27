/******************************************************************************/
/*!
\file           SpriteRenderer.hpp
\project        Lights Out
\author(s)      [Lebon], 100%
\brief          Declares SpriteRendererComponent, responsible for drawing a
                textured quad representing a GameObject. Uses the texture ID
                string stored in the owning GameObject or overrides it locally.

                Responsibilities:
                - Retrieve Texture2D from ResourceManager by texture alias
                - Bind texture and issue draw call through Renderer2D
                - Allow optional color tint and UV cropping (for spritesheets)

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#ifndef SPRITERENDERER_HPP
#define SPRITERENDERER_HPP
#include "Gameplay/GameObject.hpp"
#include "Assets/ResourceManager.hpp"
#include "Graphics/Texture2D.hpp"
#include <glm/vec4.hpp>
#include <string>

namespace Gameplay
{

  class SpriteRendererComponent : public Component
  {
  public:
    SpriteRendererComponent() = default;
    explicit SpriteRendererComponent(std::string textureAlias)
        : m_textureAlias(std::move(textureAlias)) {}

    void setTexture(const std::string &alias) { m_textureAlias = alias; }
    const std::string &textureAlias() const { return m_textureAlias; }

    void setTint(const glm::vec4 &color) { m_tint = color; }
    const glm::vec4 &tint() const { return m_tint; }

    void setUV(const glm::vec4 &uvRect) { m_uv = uvRect; }
    const glm::vec4 &uv() const { return m_uv; }

    void setIsUI(const bool& isUI) { m_isUI = isUI; }
    const bool& IsUI() const { return m_isUI; }

    void onRender(GameObject &owner) override;

    void resetAll();
  private:
    std::string m_textureAlias;
    glm::vec4 m_tint{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 m_uv{0.0f, 0.0f, 1.0f, 1.0f}; // (x,y,w,h); -1 means full texture
    bool m_isUI = false;
  };

} // namespace Gameplay

#endif
