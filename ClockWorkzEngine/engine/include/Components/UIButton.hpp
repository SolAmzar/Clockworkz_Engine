/******************************************************************************/
/*!
\file           UIButton.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          UIButton component for editor-authored interactive menu buttons.
                Attach to any entity with Transform + SpriteRenderer to make
                it a clickable button. The 'action' string is mapped to game
                logic in the active menu state.

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
    std::string uiScript{};                      // Lua script path; on_click() called on activation
    glm::vec3   normalColor{1.f, 1.f, 1.f};     // text tint when idle
    glm::vec3   hoverColor {1.f, 0.8f, 0.f};    // text tint when mouse is over
    bool        interactable{true};              // when false, ignores mouse input
    bool        showBackground{true};            // false = hide the SpriteRenderer background rect
    vec2        size{0.f, 0.f};                 // hit-test bounding box
    vec2        offset{0.f, 0.f};               // hit-area center offset from transform.position
};

} // namespace Components
