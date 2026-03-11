/******************************************************************************/
/*!
\file           UIButtonSystem.hpp
\project        ClockWorkz Engine
\author(s)      [Syafiq]
\brief          UIButtonSystem — engine-level system that handles mouse hover
                visual feedback for UIButton components.

                update(): checks mouse world position against each entity's
                hit area (transform.position + btn.offset ± btn.size*0.5),
                then applies hoverColor / normalColor to TextRenderer and
                manages SpriteRenderer visibility via showBackground.

                Must be called once per frame inside Scene::Update() so that
                hover states work in every game state, including EditorPlayState.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights
reserved.
*/
/******************************************************************************/
#pragma once
#include "ECS/World.hpp"

namespace Platform { class InputManager; }

namespace Systems {

class UIButtonSystem {
public:
    /// Handle mouse hover visual feedback. Call once per frame.
    /// Reads mouse position from InputManager and camera/window from
    /// Core::Engine::Get() so no extra parameters are needed.
    void update(ECS::World &world, Platform::InputManager &input);
};

} // namespace Systems
