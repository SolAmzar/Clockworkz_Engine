/******************************************************************************/
/*!
\file           UIButtonSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          UIButtonSystem implementation.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Systems/UIButtonSystem.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/TextRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UIButton.hpp"
#include "Core/Engine.hpp"
#include "Graphics/Camera2D.hpp"
#include "Platform/InputManager.hpp"
#include "Platform/WindowManager.hpp"
#include <GLFW/glfw3.h>

namespace Systems {

void UIButtonSystem::update(ECS::World &world, Platform::InputManager &input)
{
    Graphics::Camera2D     &cam    = Core::Engine::Get().camera();
    Platform::WindowManager &window = Core::Engine::Get().window();
    float winW = static_cast<float>(window.width());
    float winH = static_cast<float>(window.height());

    vec2 mouseWorld = input.getMouseWorldPos(cam, winW, winH);

    world.each<Components::Transform, Components::UIButton>(
        [&](ECS::Entity e, Components::Transform &tf, Components::UIButton &btn)
        {
            if (!btn.interactable) return;

            float cx    = tf.position.x + btn.offset.x;
            float cy    = tf.position.y + btn.offset.y;
            float halfW = btn.size.x * 0.5f;
            float halfH = btn.size.y * 0.5f;

            bool hovered = (mouseWorld.x >= cx - halfW && mouseWorld.x <= cx + halfW &&
                            mouseWorld.y >= cy - halfH && mouseWorld.y <= cy + halfH);

            glm::vec3 color = hovered ? btn.hoverColor : btn.normalColor;

            auto *text   = world.getComponent<Components::TextRenderer>(e);
            auto *sprite = world.getComponent<Components::SpriteRenderer>(e);

            if (text)   text->color   = color;
            if (sprite && !btn.showBackground) sprite->visible = false;
        });
}

} // namespace Systems
