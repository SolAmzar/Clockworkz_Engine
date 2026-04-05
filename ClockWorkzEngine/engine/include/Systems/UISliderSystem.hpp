/******************************************************************************/
/*!
\file           UISliderSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 93% [Lebon] 7%
\brief          UISliderSystem — handles mouse input and rendering for
                entities with Transform + UISlider components.

                update(): maps mouse world position onto slider tracks,
                updates UISlider::value, and applies changes to AudioSettings
                and AudioManager immediately.

                render(): draws track, fill, and handle quads using
                RenderingManager::drawSprite (texId 0 = solid colour).
                Must be called inside a beginScenePass / endScenePass block.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "ECS/World.hpp"

namespace Graphics  { class RenderingManager; class Camera2D; }
namespace Platform  { class InputManager; }

namespace Systems {

class UISliderSystem {
public:
    /// Handle mouse interaction.  Call once per frame before render().
    void update(ECS::World &world,
                Platform::InputManager &input,
                Graphics::Camera2D &cam,
                float winW, float winH);

    /// Draw track / fill / handle quads.
    /// Must be called between RenderingManager::beginScenePass and endScenePass.
    void render(ECS::World &world, Graphics::RenderingManager &renderer);
};

} // namespace Systems
