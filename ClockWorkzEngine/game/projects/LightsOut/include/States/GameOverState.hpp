/******************************************************************************/
/*!
\file           GameOverState.hpp
\project        Lights Out
\author(s)      [Syafiq]
\brief          Lose screen state. Loaded when the player dies.
                Loads LoseScreen.json and uses the UIButton/Lua-script system
                for Retry and Main Menu buttons. Retry reloads the exact level
                the player died on.

All content (c) 2025 DigiPen Institute of Technology Singapore. All rights reserved.
*/
/******************************************************************************/
#pragma once
#include "Runtime/GameState.hpp"
#include "Runtime/Scene.hpp"
#include "Components/UIButton.hpp"
#include "ECS/World.hpp"
#include <glm/vec2.hpp>
#include <string>
#include <vector>

namespace Platform { class InputManager; class WindowManager; }
namespace Runtime  { class GameStateManager; }
namespace Audio    { class AudioManager; }
namespace Graphics { class RenderingManager; class Camera2D; }

namespace Runtime {

class GameOverState final : public GameState {
public:
    GameOverState(GameStateManager& gsm, Platform::InputManager& input,
                  const std::string& currentLevelPath);

    const char* name() const override { return "GameOverState"; }
    void onEnter()  override;
    void onExit()   override;
    void update(double dt) override;
    void render(Core::Time* timer) override;
    void renderImGui(Core::Time* /*timer*/) override {}

private:
    void updateLayout(int w, int h);
    void handleButton(const Components::UIButton& btn);
    void collectButtons();
    void applyHoverColors();

    GameStateManager&       m_gsm;
    Platform::InputManager& m_input;
    std::string             m_currentLevelPath;  // level to reload on retry

    Graphics::RenderingManager* m_renderer{nullptr};
    Graphics::Camera2D*         m_cam{nullptr};

    Runtime::Scene           m_loseScene;
    std::vector<ECS::Entity> m_buttonEntities;
    int                      m_selected{-1};
};

} // namespace Runtime
