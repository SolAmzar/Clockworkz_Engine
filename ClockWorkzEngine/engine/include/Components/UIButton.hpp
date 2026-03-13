/******************************************************************************/
/*!
\file           UIButton.hpp
\project        Lights Out
\author(s)      [Syafiq] 68% [Sol-Amzar] 32%
\brief          UIButton component for editor-authored interactive menu buttons.
                Attach to any entity with Transform + SpriteRenderer to make
                it a clickable button. The legacy 'action' string is kept for
                compatibility with older menu scenes, while 'uiScript' supports
                script-driven buttons.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include <string>
#include "Math/Vec2.hpp"
#include <glm/vec3.hpp>

namespace Components {

struct UIButton {
    std::string action{};                        // legacy action string for older scenes
    std::string uiScript{};                      // Lua script path; on_click() called on activation
    glm::vec3   normalColor{1.f, 1.f, 1.f};
    glm::vec3   hoverColor {1.f, 0.8f, 0.f};
    bool        interactable{true};
    bool        showBackground{true};
    vec2        size{0.f, 0.f};
    vec2        offset{0.f, 0.f};
};

} // namespace Components
