/******************************************************************************/
/*!
\file           PauseState.hpp
\project        Lights Out
\author(s)      [Syafiq], 100%
\brief          Basic pause menu implementation

All content © 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include <vector>
#include <string>

namespace Platform { class InputManager; class WindowManager; }
namespace Runtime { class GameStateManager; }
namespace Audio { class AudioManager; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

    class PauseState final : public GameState {
    public:
        PauseState(GameStateManager& gsm,Platform::InputManager& input);

        const char* name() const override { return "PauseState"; }
        void onEnter() override;
        void onExit() override;
        void update(double dt) override;
        void render(Core::Time* timer) override;
        void renderImGui(Core::Time* timer) override;

    private:
        void moveUp();
        void moveDown();
        void select();

        GameStateManager& m_gsm;
        Platform::InputManager& m_input;
        Graphics::RenderingManager* m_renderer{ nullptr };
        Graphics::Camera2D* m_cam{ nullptr };

        std::vector<std::string> m_items{
            "Resume Game",
            "How To Play",
            "Main Menu"
        };

        int m_selected{ 0 };
        bool m_debug{ false };
    };

} // namespace Runtime