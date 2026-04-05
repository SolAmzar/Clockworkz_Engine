/******************************************************************************/
/*!
\file           UISliderSystem.cpp
\project        ClockWorkz Engine
\author(s)      [Syafiq] 78% [Amzar] 21% [Lebon] 7%
\brief          UISliderSystem implementation.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#include "Systems/UISliderSystem.hpp"
#include "Components/SpriteRenderer.hpp"
#include "Components/Transform.hpp"
#include "Components/UISlider.hpp"
#include "Audio/AudioManager.hpp"
#include "Audio/AudioSettings.hpp"
#include "Graphics/RenderingManager.hpp"
#include "Graphics/Camera2D.hpp"
#include "Platform/InputManager.hpp"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace Systems {

// ---------------------------------------------------------------------------
// update -- mouse-to-slider mapping and AudioSettings write-back
// ---------------------------------------------------------------------------
void UISliderSystem::update(ECS::World          &world,
                            Platform::InputManager &input,
                            Graphics::Camera2D  &cam,
                            float winW, float winH)
{
    vec2 mouseWorld = input.getMouseWorldPos(cam, winW, winH);
    float mx = mouseWorld.x;
    float my = mouseWorld.y;

    bool mouseHeld    = input.isMouseDown(GLFW_MOUSE_BUTTON_LEFT);
    bool mousePressed = input.wasMousePressed(GLFW_MOUSE_BUTTON_LEFT);

    auto &settings = Audio::AudioSettings::Get();
    auto &audio    = Audio::AudioManager::get();

    world.each<Components::Transform, Components::UISlider>(
        [&](ECS::Entity entity, Components::Transform &tf, Components::UISlider &sl)
        {
            if (auto *sprite = world.getComponent<Components::SpriteRenderer>(entity)) {
                if (!sprite->visible) {
                    sl.dragging = false;
                    return;
                }
            }

            float cx     = tf.position.x;
            float cy     = tf.position.y;
            float halfW  = sl.trackWidth  * 0.5f;
            float halfH  = sl.trackHeight * 0.5f;
            float leftX  = cx - halfW;

            float valueRange = sl.maxValue - sl.minValue;
            if (valueRange <= 0.0f) {
                valueRange = 1.0f;
            }

            // Handle center X based on current value
            float t          = (sl.value - sl.minValue) / valueRange;
            float handleX    = leftX + t * sl.trackWidth;
            float handleHalf = sl.handleSize * 0.5f;

            // Begin drag when mouse presses inside track or handle
            bool inHandle = (mx >= handleX - handleHalf && mx <= handleX + handleHalf &&
                             my >= cy - handleHalf     && my <= cy + handleHalf);
            bool inTrack  = (mx >= leftX - halfH && mx <= leftX + sl.trackWidth + halfH &&
                             my >= cy - halfH    && my <= cy + halfH);

            if (mousePressed && (inHandle || inTrack))
                sl.dragging = true;
            if (!mouseHeld)
                sl.dragging = false;

            if (sl.dragging) {
                float newT = (mx - leftX) / sl.trackWidth;
                newT = std::clamp(newT, 0.f, 1.f);
                sl.value = sl.minValue + newT * valueRange;

                // Write back to AudioSettings and AudioManager immediately
                if (sl.settingKey == "master") {
                    settings.master = sl.value;
                    audio.setGroupVolume(Audio::Group::Master, sl.value);
                } else if (sl.settingKey == "bgm") {
                    settings.bgm = sl.value;
                    audio.setGroupVolume(Audio::Group::BGM, sl.value);
                } else if (sl.settingKey == "sfx") {
                    settings.sfx = sl.value;
                    audio.setGroupVolume(Audio::Group::SFX, sl.value);
                }
            }
        });
}

// ---------------------------------------------------------------------------
// render -- draw track / fill / handle quads (inside scene pass)
// ---------------------------------------------------------------------------
void UISliderSystem::render(ECS::World              &world,
                            Graphics::RenderingManager &renderer)
{
    static const glm::vec4 UV(0.f, 0.f, 1.f, 1.f);

    world.each<Components::Transform, Components::UISlider>(
        [&](ECS::Entity entity, Components::Transform &tf, Components::UISlider &sl)
        {
            if (auto *sprite = world.getComponent<Components::SpriteRenderer>(entity)) {
                if (!sprite->visible) {
                    return;
                }
            }

            float cx    = tf.position.x;
            float cy    = tf.position.y;
            float halfW = sl.trackWidth * 0.5f;
            float leftX = cx - halfW;

            float valueRange = sl.maxValue - sl.minValue;
            if (valueRange <= 0.0f) {
                valueRange = 1.0f;
            }

            float t       = (sl.value - sl.minValue) / valueRange;
            float fillW   = t * sl.trackWidth;
            float handleX = leftX + fillW;

            // Track background
            renderer.drawSprite(0,
                glm::vec2(cx, cy),
                glm::vec2(sl.trackWidth, sl.trackHeight),
                0.f, UV, sl.trackColor, false, glm::vec2(0.f));

            // Fill bar (left-aligned, centered vertically)
            if (fillW > 0.f) {
                renderer.drawSprite(0,
                    glm::vec2(leftX + fillW * 0.5f, cy),
                    glm::vec2(fillW, sl.trackHeight),
                    0.f, UV, sl.fillColor, false, glm::vec2(0.f));
            }

            // Handle knob
            renderer.drawSprite(0,
                glm::vec2(handleX, cy),
                glm::vec2(sl.handleSize, sl.handleSize),
                0.f, UV, sl.handleColor, false, glm::vec2(0.f));
        });
}

} // namespace Systems
