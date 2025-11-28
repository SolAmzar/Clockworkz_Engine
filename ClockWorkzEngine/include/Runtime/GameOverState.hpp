/******************************************************************************/
/*!
\file           GameOverState.hpp
\project        Lights Out
\author(s)      Syafiq 100%
\brief          Declares GameOverState, shown when the player dies.
                Responsibilities include:
                - Displaying "Game Over" message
                - Providing options to restart or return to menu
                - Handling input for player choice

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/

#pragma once

#include "Runtime/GameState.hpp"
#include <string>

namespace Platform {
    class InputManager;
}

namespace Core {
    class Time;
}

namespace Runtime {
    class GameStateManager;

    class GameOverState : public GameState {
    public:
        GameOverState(GameStateManager& gsm, Platform::InputManager& input, const std::string& sceneId);
        ~GameOverState() override;

        const char* name() const override;
        void onEnter() override;
        void onExit() override;
        void update(double dt) override;
        void render(Core::Time* timer) override;
        void renderImGui(Core::Time* timer) override;

    private:
        GameStateManager& m_gsm;
        Platform::InputManager& m_input;
        std::string m_sceneId;
        float m_fadeAlpha;
        bool m_restartRequested;
        bool m_menuRequested;
    };

} // namespace Runtime