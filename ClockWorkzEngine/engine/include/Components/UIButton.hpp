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
#include <glm/vec3.hpp>

namespace Components {

struct UIButton {
    std::string action{};                        // e.g. "start_game", "exit", "resume", "main_menu"
    glm::vec3   normalColor{1.f, 1.f, 1.f};     // sprite/text tint when idle
    glm::vec3   hoverColor {1.f, 0.8f, 0.f};    // sprite/text tint when mouse is over
    bool        interactable{true};              // when false, ignores mouse input
};

} // namespace Components
